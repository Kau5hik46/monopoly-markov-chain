# Implementation Roadmap — Monopoly Markov Advisor

> **For agentic workers:** Each milestone below is its own detailed plan under
> `.kiro/specs/markov-advisor/plans/`. Implement with
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans, task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Rebuild the Monopoly simulator as a live, C++17 decision-support advisor:
a command-DSL REPL that tracks full game state, computes position-only Markov landing
probabilities, and prices single-roll rent/insurance risk for the next roller.

**Architecture:** Acyclic layers `math ← probability ← risk ← pricing`,
`domain ← rules`, `engine ← app`. Analytic Markov backend (exact) plus a Monte-Carlo
backend (rule-coupled cases + validation oracle), both behind one
`IDistributionProvider` interface.

**Tech Stack:** C++17, CMake (FetchContent GoogleTest), clang/libc++.

---

## Milestone sequence

Each milestone builds and tests independently (`ctest` green) before the next starts.

| # | Milestone | Status | Produces | Plan file | Depends on |
|---|---|---|---|---|---|
| **M1** | **Foundation** | ✅ done | CMake project, GoogleTest wired, `math` (Matrix + stationary solver), `DiceModel` (2d6 joint table + mugging probabilities), stub REPL exe | `plans/milestone-1-foundation.md` | — |
| **M2** | **Domain — board & cards** | ✅ done | `Square`, `ColorGroup`, `Board`, `BoardFactory` (London JSON), `CardDeck`/`DeckFactory` (canonical movement set) | `plans/milestone-2-domain.md` | M1 |
| **M3** | **Analytic probability engine** | ✅ done | `TransitionMatrix` (123 micro-states + card branching + 3-doubles→jail), stationary + single-roll + N-roll transient, `IDistributionProvider`, `ProbabilityEngine` facade; runnable demo in `main` | `plans/milestone-3-probability.md` | M1, M2 |
| **M2.5** | **Live-state entities** | ⬜ next | `Player`, `Bank`, `Account`, `Asset`, `GameState` (ownership/cash/houses/jail/pot) | `plans/milestone-2_5-game-state.md` | M2 |
| **M4** | **Risk & pricing** | ⬜ todo | `RentTable` (Visitor over squares), `RiskIndices` (single-roll liability), `InsurancePricer`/`OptionChain` with closed-form mugging EV (requirement A) | `plans/milestone-4-risk-pricing.md` | M2.5, M3 |
| **M5** | **Rules subsystem** | ⬜ todo | `RuleConfig` (+Builder), CoR `RuleEngine`, handlers (Mugging, AirportTravel, FreeParkingPot), `JailPolicy`, `Specification`s | `plans/milestone-5-rules.md` | M2.5 |
| **M6** | **Engine, DSL & REPL** | ⬜ todo | `Lexer`, `Parser` (Interpreter), `Command`+`Memento`, `Effect`/`CommandResult`, `CommandExecutor`, `ReadoutFormatter`, `Repl`, `Session`, `AdvisorEngine`, `main` | `plans/milestone-6-engine.md` | M2.5–M5 |
| **M7** | **Monte-Carlo backend & validation** | ⬜ todo | `MonteCarloEngine` (Prototype/Flyweight/Object-Pool), backend selector, validation-oracle tests (analytic π vs MC, rules off), golden-replay sessions per house rule | `plans/milestone-7-montecarlo.md` | M3–M6 |

## Cross-cutting acceptance gates (every milestone)
- `cmake --build build && ctest --test-dir build` is green.
- New code lives in its designated module; the layer dependency graph stays acyclic.
- Pre-commit hooks not skipped; commits are small and independently building.
- No implementation converted to TODO (including tests).

## Known decision point (surfaces at M4)
Per-property **rent schedules** must be sourced. `rents.numbers` is an Apple Numbers
binary (Snappy protobuf) and is not script-parseable. Resolve at M4 start by either
(a) exporting `rents.numbers`/`TITLE_DEEDS.numbers` to CSV, or (b) documenting a
derivation from the CSV face values using canonical Monopoly rent ratios. `data/`
JSON is the single source consumed by `BoardFactory`.

## Existing-code disposition
The current `accounts/`, `properties/`, `players/`, `math/`, `state_space/`,
`strategy/` stubs are throwaway scaffold (non-compiling). They are replaced by the new
`src/` layout and removed during M1/M2 (preserved in git history on `main`).
