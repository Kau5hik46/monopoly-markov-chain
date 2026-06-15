# Requirements — In-Game Options Trading & Session Log

Date: 2026-06-05
Status: Approved (brainstorming complete)

Extends the Markov Advisor (`.kiro/specs/markov-advisor`) from a *pricing co-pilot*
into a tool that lets players **trade** single-roll rent-insurance options in-game,
and adds a persistent session **log**. Builds on M4 (`InsurancePricer`) and M8
(`pricing::option_chain`).

## R1 — Tradeable single-roll option contract
A contract is written on a single-roll random variable with a strike/deductible `K`
and a direction (call or put). **Phase M11a** supports the **liability** underlying
(rent the insured player pays on their next roll); **Phase M11b** (R8) adds the
**income** underlying.
- **R1.1** Fields: `id`, `writer`, `holder` (long), `insured`, `type ∈ {Call, Put}`,
  `underlying ∈ {Liability, Income}`, `strike K`, `premium`, `escrow`,
  `status ∈ {Open, Matured, Settled}`, `realizedValue`.
- **R1.2** `writer = -1` means the **bank** writes it; otherwise a player writes it.
- **R1.3** `holder` may differ from `insured` (**speculation** allowed): the long bets
  on / hedges the insured player's outcome.
- **R1.4** Realized payoff at settlement, against the underlying value `U`:
  - **Call:** `max(U − K, 0)` — pays when the outcome is **above** strike (today's
    insurance is a call on liability: "rent will exceed K").
  - **Put:** `max(K − U, 0)` — pays when the outcome is **below** strike ("the insured
    will lose less than K").
  Final payout = `min(payoff, cap)` where `cap = escrow` for a peer writer and `∞` for
  the bank. For the liability underlying, `U = realizedValue` is **rent only** and
  excludes mugging transfers.

## R2 — Writers, directions, and OTC trades
- **R2.1 Bank market-maker.** Any holder can buy a call or put from the bank at the
  **fair** premium `E[payoff]` computed from the insured's exact next-roll
  distribution. Default direction is **call** (standard insurance).
- **R2.2 Peer / OTC writer.** A player writes a call **or** put to a holder at an
  **operator-specified** premium (may differ from fair); the engine still **quotes**
  the fair premium in the command echo for reference. The premium transfers
  holder → writer at open.
- **R2.3 Fair value of a put on liability** must include the no-liability mass:
  `E[max(K − L, 0)] = P(L=0)·K + Σ_{L>0} prob·max(K − L, 0)`.

## R3 — Escrow / no-default invariant
- **R3.1** When a peer writes, `escrow = maxPayoff(type, K, distribution)` is locked out
  of the writer's cash at open; opening **fails** if the writer's cash < escrow.
  - Liability **call:** `escrow = max(0, maxLoss(insured, K))`.
  - Liability **put:** `escrow = K` (payoff peaks at `U = 0`).
- **R3.2** Escrow is fixed at open. Payout is capped at escrow, so a peer writer can
  **never** default. `escrow − payout` is released to the writer at settlement.
- **R3.3** The bank posts no escrow and is always solvent.

## R4 — Lifecycle & the no-forget settlement gate
- **R4.1 Open** via `insure` (bank) or `write` (peer): validate, move premium, lock
  escrow. Contract is `Open`, bound to `insured`.
- **R4.2 Mature** (automatic): when `insured` rolls and rent resolves, every `Open`
  contract on that player flips to `Matured` and records `realizedRent`.
- **R4.3 Settle** (explicit `settle`): pays out, releases escrow, appends a ledger row,
  status → `Settled`.
- **R4.4 Gate:** while any contract is `Matured`, the executor **refuses every mutating
  command** (roll/buy/trade/write/…) with a settle prompt. Only `settle`, `undo`,
  `save`, `log`, and queries pass. Forgetting to settle is structurally impossible.

## R5 — Consistency with undo / save-load / rollback
Contracts and the ledger live in `GameState`, so existing undo (Memento snapshots),
save/load (JSON), and failed-mutation rollback cover them automatically and atomically.

## R6 — Session log & ledger
- **R6.1** A chronological journal records every executed command (echoed text +
  causal effects + turn marker). `log [N]` prints it (last `N`, default all).
- **R6.2** The journal is appended to a session file on disk (append-only, flushed per
  command), surviving the session.
- **R6.3** `query ledger` renders a financial view: per-contract premium/escrow/payout
  and net P&L per player.

## R7 — Tests (per `.kiro/steering/testing.md`)
Cash conservation at open/settle; `payout ≤ escrow` no-default invariant; gate blocks
mutations while matured; undo/save-load round-trip of contracts+ledger; zero-rent
maturity; bank vs peer settlement; speculation (holder ≠ insured); self-hedge default;
cross-check that bank fair premium at `K=0` == `query chain` premium(0) ==
`InsurancePricer.expectedRent`.

## R8 — Income options (Phase M11b, designed-for)
A call/put written on the **income** an owner `Py` collects, accounting for the fact
that **any** opponent within reach may land on a `Py`-owned square.
- **R8.1 Underlying.** `U = total rent Py collects over the turn-aware window` — every
  rent payment Py receives from now until it is `Py`'s turn again (doubles re-rolls
  included). The engine builds each reachable opponent's rent-to-`Py` distribution and
  **convolves** them into the total-income distribution.
- **R8.2 Lander set.** By default the engine **auto-detects** every opponent who could
  land on a `Py`-owned square next roll (within 2–12 squares behind it); the operator
  may optionally **override** with an explicit subset on the command.
- **R8.3 Incremental maturity.** The contract accumulates rent paid to `Py` through the
  existing rent-resolution path and **fully matures** when `nextRoller` cycles back to
  `Py`; then the settlement gate (R4.4) applies.
- **R8.4 Escrow.** A documented **conservative upper bound** on total income (per
  referenced opponent: `2 × worst-case single rent to Py`). Payout is still capped at
  escrow (R3.2), preserving no-default even in extreme doubles chains; the bound is
  echoed at open.
- **R8.5 Pricing.** Fair value `E[payoff]` over the convolved income distribution, plus
  the realized payoff at settlement.

## Out of scope
Secondary resale of open contracts, GUI, contracts spanning more than one of `Py`'s
turns. Exotic structures (digital/binary, straddles, spreads) are deferred unless
explicitly requested.
