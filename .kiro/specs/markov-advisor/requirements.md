# Requirements — Monopoly Markov Advisor

Date: 2026-06-04
Status: Approved (brainstorming complete)

## R1 — Live game loop (REPL)
The tool runs as a read-eval-print loop. The operator types real, in-progress game
events one per line using a formal command DSL. After each command the tool applies
the event to the tracked game state and prints output (see R7).

## R2 — Full game state mechanics
The tool maintains complete game ground truth: per-player board position, cash,
property ownership, houses/hotels, mortgages, jail status; the bank's house/hotel
supply; the Free-Parking house pot; whose turn it is and who rolls next.
This state is the single source of truth and is mutated only by commands.

## R3 — Position-only Markov probability engine
A Markov chain over **board position only** computes landing distributions:
- **R3.1** ~123 micro-states encoding square (0–39) × consecutive-doubles (0–2) and
  in-jail (attempts 0–2); the 3-consecutive-doubles→jail rule is modeled exactly.
- **R3.2** Full Chance/Community-Chest **movement-card** branching, modeled per card
  square (each card square's "nearest", "back 3", "advance to" targets differ).
- **R3.3** **Stationary** distribution `π` (solve `πP = π`).
- **R3.4** **Transient** distribution `pₙ = e₀·Pⁿ` from a known current position for
  the next N rolls.
- The probability engine knows nothing about money.

## R4 — Monte-Carlo backend
A second probability backend simulates full mechanics (including coupling rules) by
cloning the live state and rolling forward. It is selected automatically when active
rules couple player movement, and serves as a validation oracle for the analytic
engine when coupling rules are off. Both backends expose one common interface.

## R5 — Risk & valuation layer
Overlays live ownership/cash/houses onto position probabilities to produce indices:
- **R5.1** Expected rent liability for the next roller (single roll) — **v1 primary**.
- **R5.2** Property valuation from `π` (long-run landing × rent).
- **R5.3** (Designed-for) N-roll cumulative liability and ruin probability.

## R6 — Option / insurance pricing
- **R6.1 (v1)** Fair premium for insuring the **next single roll's rent liability**
  for the next roller:
  `Σ_s L(s)·rent_owed_if_opponent(s) + Σ_{occupied,eligible} L(s)·MuggingEV`.
  Computed exactly from known positions, including signed mugging EV.
- **R6.2 (designed-for)** Strike-based option chain `E[max(rent − K, 0)]`, N-roll,
  and bankruptcy/ruin insurance.

## R7 — Command side effects & always-on rendering
- **R7.1** Each command returns a structured `CommandResult` containing the ordered,
  causal list of `Effect`s it produced (moves, rent, mugging contest + outcome,
  sent-to-jail/hospital, tax-to-pot, pot-claim, house-built, airport-travel,
  bankruptcy, cash transfers).
- **R7.2** After every command the tool prints to `cout`: (1) the echoed/parsed
  command, (2) the side-effect log in plain English in causal order, (3) the full
  game-state panel, (4) the advisory readout (risk + single-roll option premium) for
  the next roller. A `--quiet` flag may suppress (3)/(4) for scripted runs; default
  shows all.

## R8 — Configurable house rules
A first-class rules subsystem with independently toggleable, parameterized rules:
- **R8.1 Mugging** — opponent-occupied square (excl. Jail, Free Parking) → separate
  2d6 contest; mugger wins on `>` (£500K from muggee, muggee→Free Parking); muggee
  wins on `≥`, ties to muggee (mugger→Jail).
- **R8.2 Airport travel** — landing on an owned airport allows optional travel to
  another owned airport, costing the next turn (skip).
- **R8.3 Free-Parking house pot** — Income Tax→+2 houses, Super Tax→+1 house, drawn
  from the bank's 32/12 supply; claimer places on owned monopolies per build rules;
  unplaceable houses convert to cash at `count × build cost`.
- **R8.4 Jail policy** — attempt doubles ≤ 3 turns then pay & move (configurable).
- Rules are added as new Chain-of-Responsibility handlers without editing existing
  ones; toggled at runtime via `rules <name> on|off` and loaded from
  `data/rules.default.json`.

## R9 — London edition data
Board names, positions, color groups, prices, rent schedules, house costs, mortgage
values, and the Chance/Community-Chest decks come from `data/` config files for the
London-themed variant. (Rent schedules to be sourced from `rents.numbers`.)

## R10 — Determinism & reproducibility
All randomness flows through an injected seeded RNG. Sessions are saveable/replayable
(`save`/`load`), and golden replays produce deterministic effect logs and panels.

## R11 — Quality bars
C++17; CMake build; GoogleTest across unit/invariant/validation/golden tiers;
acyclic module dependencies; pre-commit hooks not skipped; work committed in
independently building increments.
