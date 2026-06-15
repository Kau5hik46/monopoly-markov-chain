# Design — In-Game Options Trading & Session Log

Date: 2026-06-05
Status: Approved (brainstorming complete)

## Overview
Adds a tradeable single-roll rent-insurance option (bank- or peer-written, with a
strike `K` and escrow-guaranteed settlement), an enforced settlement gate, and a
persistent session log + financial ledger. Reuses the existing exact next-roll loss
distribution from `pricing::option_chain`.

## Architecture
New `src/contracts/` module (pure logic), depended on by `engine`, depending on
`pricing` + `domain` (acyclic, mirrors existing layering). Financial state lives in
`GameState` so undo / save-load / rollback cover it for free.

```
domain (GameState += contracts, ledger)
   ^                ^
pricing (option_chain += fairPremiumAtStrike, maxLoss)
   ^
contracts (option_book: open / mature / settle / escrow)
   ^
engine (parser, executor, query_service, help, session, repl)
```

## Data model (in `domain/game_state.h`)
```cpp
enum class ContractStatus { Open, Matured, Settled };
enum class OptionType { Call, Put };          // direction
enum class Underlying { Liability, Income };  // Income == Phase M11b
struct OptionContract {
  int  id = 0;
  int  writer = -1;        // -1 == bank
  int  holder = -1;        // the long
  int  insured = -1;       // liability: roller; income: owner Py
  OptionType type = OptionType::Call;
  Underlying underlying = Underlying::Liability;
  long strike = 0;         // K
  long premium = 0;        // paid holder -> writer at open
  long escrow = 0;         // locked from a peer writer (0 for bank)
  ContractStatus status = ContractStatus::Open;
  long realizedValue = 0;  // liability: rent insured paid; income: rent Py collected
  // Income only (M11b): referenced landers + how many have rolled so far.
  std::vector<int> landers;
  int landersRolled = 0;
};
struct LedgerEntry {       // appended at settle
  int id, writer, holder, insured;
  OptionType type; Underlying underlying;
  long strike, premium, escrow, payout;  // payout = writer -> holder
};
// GameState gains: std::vector<OptionContract> contracts; std::vector<LedgerEntry> ledger;
//                  int nextContractId = 1;
```

## `contracts::OptionBook` (pure functions over GameState)
- `openBank(gs, holder, insured, K, resolver) -> result` — premium = fair at K; move
  premium holder→bank (cash leaves holder; bank is implicit). Append `Open` contract.
- `openPeer(gs, writer, holder, insured, K, premium, resolver) -> result` — compute
  `escrow = max(0, maxLoss(insured,K))`; fail if `writer.cash < escrow`; move premium
  holder→writer; lock escrow (subtract from writer.cash). Append `Open` contract.
  `openBank`/`openPeer` take an `OptionType type` (and, in M11b, `Underlying`); premium
  / escrow / fair quote computed per the **Payoff & escrow** rules below.
- `matureOnRoll(gs, insured, realizedValue)` — flip all `Open` liability contracts on
  `insured` to `Matured`, record `realizedValue` (rent paid). Called from roll
  resolution. (M11b adds income accumulation across the turn-aware window.)
- `settle(gs, id | all) -> result` — `payoff = (type==Call) ? max(realizedValue−K,0)
  : max(K−realizedValue,0)`; `payout = min(payoff, cap)`; writer→holder pays payout;
  release `escrow−payout` to peer writer; append ledger row; status→`Settled`.
- `hasMatured(gs) -> bool` — gate predicate.

## Pricing helpers (in `pricing/option_chain`)
```cpp
// Call fair value E[max(L-K,0)].
double fairPremiumAtStrike(const std::vector<LossOutcome>& loss, double K);
// Generalized fair value for call OR put. The put branch adds the no-liability mass:
//   put = P(L=0)*K + Σ_{L>0} prob*max(K-L,0),  where P(L=0) = 1 - Σ loss prob.
double fairValueAtStrike(const std::vector<LossOutcome>& loss, double K, bool isPut);
double maxLossOf(const std::vector<LossOutcome>& loss);
```
All derive from the existing `buildLossDistribution(gs, insured, resolver)`. No
duplication of the 36-outcome × card-branch enumeration. `fairPremiumAtStrike` is the
call case of `fairValueAtStrike`.

## Payoff & escrow (liability underlying, M11a)
- **Realized payoff:** call `max(U−K,0)`, put `max(K−U,0)`, `U = realizedValue`.
- **Fair value (bank premium / OTC quote):** `fairValueAtStrike(loss, K, isPut)`.
- **Escrow (peer writer):** call `max(0, maxLossOf(loss) − K)`; put `K`.
- **Settlement payout:** `min(payoff, escrow_or_∞)` — cap preserves no-default.

## DSL (extends `engine/parser.cpp`, `command.h`)
| Command | Parse → Command |
|---|---|
| `insure P{h} [call\|put] [P{ins}] [strike <amt>]` | `Insure`: holder, type (default call), insured (default holder), strike (default 0) |
| `write P{w} -> P{h} [call\|put] [P{ins}] [strike <amt>] premium <amt>` | `Write`: writer, holder, type, insured, strike, premium |
| `settle <id>` / `settle all` | `Settle`: id or all-flag |
| `log [N]` | `Log`: count (0 = all) |
| `query ledger` | `QueryKind::Ledger` |

`call`/`put`/`strike`/`premium` are word-keyword tokens (like `vs`, `on`); amounts via
existing `parseAmount`; players via `parsePlayer`. New `CommandKind`: `Insure, Write,
Settle, Log`. New `QueryKind::Ledger`. The `write` echo includes the engine's **fair
quote** alongside the negotiated premium (R2.2).

**Phase M11b income syntax (designed-for):**
`write P{w} -> P{h} call income P{Py} strike <amt> premium <amt> [landers P{a} P{b} …]`
— omitting `landers` auto-detects reachable opponents (R8.2).

## Executor integration (`engine/executor.cpp`)
- `Insure`/`Write`/`Settle` cases call `OptionBook`; all are mutating ⇒ snapshot +
  rollback path already applies.
- **Gate:** at the top of the mutating section, if `OptionBook::hasMatured(gs_)` and
  the command is not `Settle`/`Undo`/`Save`/`Log`/`Query`, fail with
  `"settlement required — settle #<id> (P{ins} rent <£>, strike <£> → payout <£>); or settle all"`
  plus a `settle` prompt.
- **Maturity hook:** in `resolveLanding` (or right after roll rent is applied), after
  computing the rent the insured paid, call `matureOnRoll(gs_, insured, rent)`. Emit a
  `Matured`/settle prompt effect.

## Log & ledger (`engine/repl.cpp`, `engine/query_service.cpp`)
- `JournalEntry{seq, commandText, effects, turnMarker}` accumulated in `Repl`; appended
  to `<session>.log` (defaults to `./monopoly-session.log`; if a save path is known,
  sits beside it). Append-only, flushed per command.
- `log [N]` renders the journal (handled in executor returning effects, or in repl
  render path — journal owned by repl, so `Log` is handled in `Repl::run` before
  dispatch, or executor exposes a journal ref). **Decision:** journal lives in `Repl`;
  `log` is intercepted in `Repl::run`.
- `query ledger` renders `gs.ledger` + open contracts: premium collected, escrow
  locked, payout, net per player.

## Session save/load (`engine/session.cpp`)
Serialize/deserialize `contracts` (incl. `type`, `underlying`, `landers`,
`landersRolled`), `ledger`, `nextContractId` in the existing JSON.

## Phase M11b — Income options (designed-for; built after M11a)
- **Underlying build (`pricing`):** `buildIncomeDistribution(gs, owner, landers,
  resolver)` — for each lander, enumerate their 36 outcomes × card branches, keep only
  landings on `owner`-owned squares, weight rent paid to `owner`; **convolve** the
  per-lander discrete distributions into the total-income distribution. Reachable-lander
  auto-detection: any opponent whose position is 2–12 squares behind some `owner` square.
- **Fair value / escrow:** `fairValueAtStrike` reused on the income distribution;
  escrow = conservative bound `Σ_landers 2 × maxRentToOwner(lander)` (R8.4).
- **Maturity (`executor`):** an income contract on `Py` accumulates `realizedValue`
  whenever a referenced lander pays rent to `Py` (hook in `resolveLanding`'s rent
  branch); `landersRolled` increments per referenced lander's roll; contract flips to
  `Matured` when `nextRoller` returns to `Py` (turn-aware window, R8.3). Gate then applies.
- **DSL:** `... call income P{Py} … [landers …]` (see DSL section).
- **Tests:** convolution correctness vs. `query simulate` Monte-Carlo; auto-lander
  detection; turn-aware maturity across a full round incl. doubles; escrow ≥ realized
  income on randomized boards (no-default); put/call symmetry on income.

## Error handling
- Open fails (returns `r.fail`, rolled back) on: unknown player, peer writer cash <
  escrow, strike < 0, holder == writer for a peer write (no self-trade).
- Settle fails on unknown/already-settled id, or `settle` when nothing is matured.
- Gate failure returns a clear, actionable settle prompt.

## Invariants (pinned by tests)
1. Open: `holder.cash −= premium`, `writer.cash += premium`; peer `writer.cash −=
   escrow`. Total cash (players + escrow + bank) conserved.
2. `payout ≤ escrow` for peer writers (no default), for any board state.
3. Settle: `writer.cash += (escrow − payout)`, `holder.cash += payout`.
4. Gate: no mutating command succeeds while a `Matured` contract exists.
5. Undo reverses open/mature/settle atomically; save/load round-trips state.
6. Bank fair premium at `K=0` == `option_chain` premium(0) == `InsurancePricer`
   `expectedRent` for the same insured.

## Files (M11a)
New: `src/contracts/option_book.{h,cpp}`, `tests/unit/test_pricing_strike.cpp`,
`test_option_book.cpp`, `test_options_dsl.cpp`, `test_options_exec.cpp`,
`test_options_session.cpp`, `test_options_log.cpp`.
Edited: `pricing/option_chain.{h,cpp}` (+`fairValueAtStrike`), `domain/game_state.h`
(+`OptionType`/`Underlying`/contract+ledger), `engine/{command.h, parser.cpp,
executor.{h,cpp}, query_service.{h,cpp}, help.cpp, session.cpp, repl.{h,cpp}}`,
`CMakeLists.txt`. Each file stays < 400 LoC. (`OptionContract`/`LedgerEntry` POD types
live in `game_state.h`; `contracts` holds only logic.)

## Invariants — extra for put/call
7. Put on liability: payoff `max(K−U,0)`; fair value uses the no-liability mass (R2.3);
   peer escrow `= K`; `payout ≤ K`.
8. Call/put symmetry: at a strike `K` where `P(U=0)=0`, `callFV(K)+K == putFV(K)+E[U]`
   (put-call parity sanity check on the discrete distribution).
