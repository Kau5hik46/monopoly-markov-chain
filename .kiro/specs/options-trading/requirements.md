# Requirements — In-Game Options Trading & Session Log

Date: 2026-06-05
Status: Approved (brainstorming complete)

Extends the Markov Advisor (`.kiro/specs/markov-advisor`) from a *pricing co-pilot*
into a tool that lets players **trade** single-roll rent-insurance options in-game,
and adds a persistent session **log**. Builds on M4 (`InsurancePricer`) and M8
(`pricing::option_chain`).

## R1 — Tradeable single-roll option contract
A contract insures one player's **next single roll** of rent liability, with a
strike/deductible `K`:
- **R1.1** Fields: `id`, `writer`, `holder` (long), `insured`, `strike K`, `premium`,
  `escrow`, `status ∈ {Open, Matured, Settled}`, `realizedRent`.
- **R1.2** `writer = -1` means the **bank** writes it; otherwise a player writes it.
- **R1.3** `holder` may differ from `insured` (**speculation** allowed): the long bets
  on / hedges the insured player's roll.
- **R1.4** Payout at settlement = `min(max(realizedRent − K, 0), cap)` where `cap =
  escrow` for a peer writer and `∞` for the bank. `realizedRent` is **rent only** and
  excludes mugging transfers.

## R2 — Writers: bank and peers
- **R2.1 Bank market-maker.** Any holder can buy from the bank at the **fair** premium
  `E[max(L − K, 0)]` computed from the insured's exact next-roll loss distribution.
- **R2.2 Peer writer.** A player writes to a holder at an **operator-specified**
  premium (may differ from fair). The premium transfers holder → writer at open.

## R3 — Escrow / no-default invariant
- **R3.1** When a peer writes, `escrow = max(0, maxLoss(insured, K))` is locked out of
  the writer's cash at open; opening **fails** if the writer's cash < escrow.
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

## Out of scope
N-roll / cumulative contracts, secondary resale of open contracts, GUI. Single-roll
European only, matching the exact-pricing guarantee.
