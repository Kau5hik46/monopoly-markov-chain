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
struct OptionContract {
  int  id = 0;
  int  writer = -1;        // -1 == bank
  int  holder = -1;        // the long
  int  insured = -1;       // whose roll triggers it
  long strike = 0;         // K
  long premium = 0;        // paid holder -> writer at open
  long escrow = 0;         // locked from a peer writer (0 for bank)
  ContractStatus status = ContractStatus::Open;
  long realizedRent = 0;   // filled at Mature
};
struct LedgerEntry {       // appended at settle
  int id, writer, holder, insured;
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
- `matureOnRoll(gs, insured, realizedRent)` — flip all `Open` contracts on `insured`
  to `Matured`, record `realizedRent`. Called from roll resolution.
- `settle(gs, id | all) -> result` — `payout = min(max(realizedRent−K,0), cap)`;
  writer→holder pays payout; release `escrow−payout` to peer writer; append ledger
  row; status→`Settled`.
- `hasMatured(gs) -> bool` — gate predicate.

## Pricing helpers (in `pricing/option_chain`)
```cpp
double fairPremiumAtStrike(const std::vector<LossOutcome>& loss, double K);
double maxLossOf(const std::vector<LossOutcome>& loss);
```
Both derive from the existing `buildLossDistribution(gs, insured, resolver)`. No
duplication of the 36-outcome × card-branch enumeration.

## DSL (extends `engine/parser.cpp`, `command.h`)
| Command | Parse → Command |
|---|---|
| `insure P{h} [P{ins}] [strike <amt>]` | `Insure`: holder, optional insured (default holder), strike (default 0) |
| `write P{w} -> P{h} [P{ins}] [strike <amt>] premium <amt>` | `Write`: writer, holder, insured, strike, premium |
| `settle <id>` / `settle all` | `Settle`: id or all-flag |
| `log [N]` | `Log`: count (0 = all) |
| `query ledger` | `QueryKind::Ledger` |

`strike` and `premium` are word-keyword tokens (like `vs`, `on`); amounts via existing
`parseAmount`; players via `parsePlayer`. New `CommandKind`: `Insure, Write, Settle,
Log`. New `QueryKind::Ledger`.

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
Serialize/deserialize `contracts`, `ledger`, `nextContractId` in the existing JSON.

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

## Files
New: `src/contracts/contract.h`, `src/contracts/option_book.{h,cpp}`,
`tests/unit/test_option_book.cpp`, `tests/unit/test_options_trading_dsl.cpp`.
Edited: `pricing/option_chain.{h,cpp}`, `domain/game_state.{h,cpp}`,
`engine/{command.h, parser.cpp, executor.{h,cpp}, query_service.{h,cpp}, help.cpp,
session.cpp, repl.{h,cpp}}`, `CMakeLists.txt`. Each file stays < 400 LoC.
