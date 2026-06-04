# Tech Steering — Monopoly Markov Advisor

## Language & standard
- **C++17**. Use `std::variant`/`std::visit`, `std::optional`, `std::string_view`,
  `std::array`, structured bindings, `if constexpr`, `constexpr` tables,
  `[[nodiscard]]`. No exceptions for control flow in hot paths; prefer
  `std::optional`/result types.

## Build & layout
- **CMake** (>= 3.16). Targets:
  - `monopoly_core` — static library (all subsystems).
  - `monopoly` — REPL executable.
  - `monopoly_tests` — GoogleTest executable.
- Out-of-source build in `build/`. C++17 enforced via `target_compile_features`.
- Module layout (acyclic dependency: `math ← probability ← risk ← pricing`;
  `domain ← rules`; `engine` depends on all; `app` depends on `engine`):

```
src/
  math/         Matrix, Vector, linear/eigen solver
  domain/       Board, Square (variant), TitleDeed, ColorGroup, Player, Bank,
                Account, Asset, GameState (aggregate root)
  rules/        RuleConfig, Rule (Chain of Responsibility handlers), RuleEngine,
                JailPolicy, Specifications
  probability/  DiceModel, TransitionMatrix/MarkovChain, StationarySolver,
                TransientSolver, MonteCarloEngine, IDistributionProvider (Bridge),
                ProbabilityEngine (Facade + selector)
  risk/         RentTable, RiskIndices
  pricing/      InsurancePricer, OptionChain
  engine/       Lexer, Parser (Interpreter), Command (+ Memento), CommandExecutor,
                Effect, CommandResult, ReadoutFormatter, Repl, Session
  app/          main.cpp
data/           board.london.json, decks.london.json, rules.default.json
tests/          unit/, invariants/, validation/, golden/
.kiro/          steering/, specs/
```

## Design patterns (locked)
Interpreter (DSL), Command + Memento (events/undo), Facade (`AdvisorEngine`,
`ProbabilityEngine`), Composite (`ColorGroup`/monopoly), State (player/jail),
Visitor (square landing & rent), Specification (rule predicates), Null Object
(unowned → Bank), Chain of Responsibility (rules), Strategy (policies/models),
Builder (`RuleConfig`), Bridge (`IDistributionProvider`), Flyweight (shared board
data), Prototype (clone state for rollouts), Object Pool (rollout reuse), Template
Method (turn-resolution skeleton shared by executor & simulator), Factory
(`BoardFactory`/`DeckFactory`), Observer (recompute on state change).

## Performance
- Transition matrix (~123 micro-states) built once, stored row-major in a
  contiguous `std::vector<double>`; power-iteration for `π`.
- Monte-Carlo uses Flyweight immutable board + Prototype clone of mutable state +
  Object Pool to avoid per-rollout allocation. Reproducible via seeded RNG passed
  in (no `Math.random`/global state).
- Single-roll analytic path is O(squares) and runs every command.

## Determinism
- All randomness flows through an injected seeded RNG so sessions and tests are
  reproducible. The live tool consumes operator-provided dice, not RNG.

## Tooling rules
- Do not skip pre-commit hooks.
- Do not add Claude as commit author/co-author.
- Break work into independently committable, building tasks.
