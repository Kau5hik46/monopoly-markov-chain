# Design — Monopoly Markov Advisor

Date: 2026-06-04
Status: Approved (brainstorming complete)
Related: ./requirements.md, ../../steering/product.md, ../../steering/tech.md,
../../steering/testing.md

## 1. Architecture

Three logical layers plus I/O, with a strictly acyclic dependency graph:

```
math ← probability ← risk ← pricing
domain ← rules
engine (depends on all)  ←  app
```

- **domain/** — full game mechanics, the source of truth.
- **probability/** — position-only engine (analytic Markov + Monte-Carlo backends).
- **risk/** + **pricing/** — overlay live state onto probabilities to produce indices
  and prices.
- **rules/** — configurable house-rules subsystem read by both engine and probability.
- **engine/** — DSL interpreter, command execution, effects, rendering, REPL.
- **app/** — entry point.

### 1.1 Module responsibilities

| Module | Key types | Responsibility |
|---|---|---|
| `math` | `Matrix`, `Vector`, `solveStationary`, `powerIterate` | Linear algebra for the chain. Replaces the old `math/vector.h` stub. |
| `domain` | `Board`, `Square` (variant), `TitleDeed`, `ColorGroup`, `Player`, `Bank`, `Account`, `Asset`, `GameState` | Entities + aggregate root. `GameState` is mutated only via commands. |
| `rules` | `RuleConfig`, `Rule` (CoR handler), `RuleEngine`, `JailPolicy`, `Specification`s | Toggleable house rules as an ordered handler chain. |
| `probability` | `DiceModel`, `TransitionMatrix`, `StationarySolver`, `TransientSolver`, `MonteCarloEngine`, `IDistributionProvider`, `ProbabilityEngine` | Landing distributions, position-only. Selects analytic vs MC. |
| `risk` | `RentTable`, `RiskIndices` | Expected rent liability, valuation, ruin (designed-for). |
| `pricing` | `InsurancePricer`, `OptionChain` | Fair premium for the next roll. |
| `engine` | `Lexer`, `Parser`, `Command`, `Effect`, `CommandResult`, `CommandExecutor`, `ReadoutFormatter`, `Repl`, `Session`, `AdvisorEngine` | DSL → commands → effects → render → loop. |
| `app` | `main` | Wire dependencies, start REPL. |

## 2. Design patterns (locked)

- **Interpreter** — DSL grammar; each production is an expression node compiled to a
  `Command`.
- **Command + Memento** — events apply/undo; `GameState` snapshots for undo and MC
  seeding.
- **Facade** — `AdvisorEngine` (single REPL entry point), `ProbabilityEngine` (over
  the two backends).
- **Composite** — `ColorGroup` aggregates properties; monopoly & build-legality are
  queries over it.
- **State** — `Player` status (`Active`/`InJail(turns,attempts)`/`Bankrupt`) and jail
  behavior.
- **Visitor** — landing effects and rent over the `Square` variant via `std::visit`.
- **Specification** — composable predicates (`IsMonopoly`, `CanBuildHere`,
  `IsMuggingEligible`, `IsAirportTravelLegal`).
- **Null Object** — unowned property owner is the Bank (no null checks).
- **Chain of Responsibility** — rules pipeline `Mugging → FreeParkingPot → Tax →
  Rent → CardDraw`; membership from `RuleConfig`.
- **Strategy** — `JailPolicy`, `MuggingResolution`, `AirportTravelPolicy`,
  `RiskModel`, `PricingModel`.
- **Builder** — fluent `RuleConfig` builder.
- **Bridge** — `IDistributionProvider` abstraction vs `AnalyticMarkov`/`MonteCarlo`.
- **Flyweight** — shared immutable board/title-deed/deck data across rollouts.
- **Prototype** — clone live `GameState` to seed each MC rollout.
- **Object Pool** — recycle rollout state objects.
- **Template Method** — shared turn-resolution skeleton used by both the live
  executor and the simulator so they cannot drift.
- **Factory** — `BoardFactory`, `DeckFactory`, `SquareFactory`.
- **Observer** — recompute indices on `GameState` change.

## 3. The math model

### 3.1 Micro-states
- On-board: `(square s ∈ 0..39, d ∈ {0,1,2})` — `d` = consecutive doubles this turn.
- In-jail: `(InJail, attempts ∈ {0,1,2})`.
≈123 states. A non-double ends the turn (`d→0`); a double increments `d` and re-rolls;
the 3rd double routes to jail. Marginalizing `d` and folding `InJail→10` recovers the
40-square distribution.

### 3.2 Dice model
`constexpr` 2d6 joint table `P(sum=k, isDouble)` (sums 2–12, triangular; doubles
{2,4,6,8,10,12} each 1/36). Mugging contest reuses the 2d6 sum table:
`P(mugger>muggee) ≈ 0.4437`, `P(tie) ≈ 0.1127`, `P(muggee≥) ≈ 0.5563`.

### 3.3 Card branching
Chance (7,22,36) and Community Chest (2,17,33) modeled per square: each card-square's
matrix row is a probability mixture over its deck's movement outcomes (non-movers =
stay). "Nearest airport/utility", "back 3", "advance to X" resolve per source square.
Decks in `data/decks.london.json`, built by `DeckFactory`.

### 3.4 Solvers (analytic backend)
- **Stationary `π`**: power iteration to convergence; cross-checked by a direct
  `(Pᵀ−I)` linear solve with normalization. Drives valuation.
- **Transient `pₙ = e₀·Pⁿ`**: from the roller's known micro-state, `n = 1..N`.

### 3.5 Requirement-A pricing
Single-throw landing distribution `L(s) = P(land on s | one 2d6 throw)` (with card
branching + GO_TO_JAIL redirect). Fair single-roll premium:
```
premium = Σ_s L(s) · rent_owed_if_opponent(s, GameState)
        + Σ_{s occupied by opponent & mugging-eligible} L(s) · MuggingEV(roller, occupant)
MuggingEV = 0.4437·(+£500K gain, victim→Free Parking) − 0.5563·(roller→Jail cost)
```
`rent_owed` from the risk layer (owner/houses/mortgage/station-count/utility-mult).
The strike chain (R6.2) later swaps the linear payoff for `E[max(rent−K,0)]` over the
same `L(s)`.

### 3.6 Monte-Carlo backend
When coupling rules are active, or for N-roll/stationary under them, clone live
`GameState` (Prototype + Flyweight + Object Pool) and run K seeded rollouts applying
full mechanics + policies; aggregate landing freq, rent paid, ruin, mugging outcomes;
return via `IDistributionProvider` with confidence intervals.

**Selector**: single-roll-from-known-positions is always exact analytically (mugging
EV layered closed-form). Only multi-roll/long-run under coupling rules use MC. Rules
off ⇒ MC must match analytic `π` within tolerance (standing validation test).

## 4. Rules subsystem

`RuleConfig` (from `data/rules.default.json`, runtime-toggle via `rules` command)
drives a Chain-of-Responsibility of landing handlers; each rule is a toggleable
Strategy. Chain order: `Mugging → FreeParkingPot → Tax → Rent → CardDraw`.

| Rule | Effect | Params |
|---|---|---|
| Mugging | opponent-occupied (excl. Jail/Free Parking) → 2d6 contest; mugger `>` wins £500K + victim→Free Parking; muggee `≥` wins → mugger→Jail | `enabled, amount=500K, eligibleSquares, hospital=FreeParking` |
| AirportTravel | owned airport → optional travel to another owned airport, skip next turn | `enabled` |
| FreeParkingPot | Income Tax→+2 houses, Super Tax→+1; from 32/12 supply; claimer places on monopolies, unplaceable → cash `count×buildCost` | `enabled, perIncomeTax=2, perSuperTax=1, respectSupply=true` |
| JailPolicy | attempt doubles ≤3 turns then pay & move | `maxAttempts=3, fine` |

New rules = new handler; no edits to existing handlers.

## 5. Command DSL (Interpreter)

One verb per line. `=` value, `@` location, `->` transfer, `:` outcome. Squares are
name tokens (`@CANARY_WHARF`) or positions (`@#37`); amounts accept `K`/`M`.

```ebnf
line       := statement | query | control
statement  := roll | move | buy | sell | trade | rent | build | mortgage
            | unmortgage | tax | card | jail | mug | airport | claim | bankrupt | cash | init
roll       := "roll" player "=" int "," int
move       := "move" player "->" "@" square
buy        := "buy" player "@" square [ "=" amount ]
sell       := "sell" player "->" player "@" square [ "=" amount ]
trade      := "trade" player "<->" player ":" bundle "<->" bundle
rent       := "rent" player "->" player "@" square [ "=" amount ]
build      := "build" player "@" square ("+"|"-") int
mortgage   := "mortgage"   player "@" square
unmortgage := "unmortgage" player "@" square
tax        := "tax"  player "=" amount "@" ("INCOME"|"SUPER")
card       := "card" player ":" cardEffect
jail       := "jail" player ("+"|"-")
mug        := "mug"  player "vs" player "=" int ":" int
airport    := "airport" player "@" square "->" "@" square
claim      := "claim" player { "@" square "+" int }
bankrupt   := "bankrupt" player "->" (player | "BANK")
cash       := "cash" player ("+="|"-=") amount
init       := "init" player { "," player }
query      := "query" ("risk"|"options"|"dist"|"stationary"|"value"|"state")
                       [ player | "@" square | "^" int ]
control    := "undo" | "redo" | "save" string | "load" string
            | "rules" ident ("on"|"off")
player     := "P" int
square     := IDENT | "#" int
amount     := int [ "K" | "M" ]
```

Each production compiles to a `Command` (apply/undo).

## 6. Effects & always-on rendering

- `Command::execute(GameState&) → CommandResult{ vector<Effect>, status }`.
- `Effect` is a typed, ordered, causal record: `Moved`, `PassedGo`, `RentPaid`,
  `CashTransfer`, `MuggingContest{rolls,winner}`, `SentToJail`, `SentToHospital`,
  `TaxToPot`, `PotClaimed`, `HouseBuilt`, `AirportTravel`, `Bankrupt`, … CoR handlers
  and the ledger append effects as they fire.
- After every command `ReadoutFormatter` prints to `cout`:
  1. echoed/parsed command (or structured parse error),
  2. the side-effect log in plain English (causal order),
  3. the full game-state panel (positions, cash, holdings, houses, jail, pot, bank
     supply, whose turn / next roller),
  4. advisory readout (risk + single-roll option premium) for the next roller.
- `--quiet` suppresses (3)/(4) for scripted runs; default shows all.
- The ordered `Effect` list and the panel render are the assertion surfaces for
  golden-replay/snapshot tests.

## 7. Data files (London edition)

- `data/board.london.json` — 40 squares: name, position, type, color group, price,
  rent schedule (site/1–4/hotel), house cost, mortgage value. Rent schedules sourced
  from `rents.numbers`.
- `data/decks.london.json` — Chance / Community-Chest cards with movement effects.
- `data/rules.default.json` — default `RuleConfig`.

## 8. Testing

Per `../../steering/testing.md`: Tier 1 unit, Tier 2 invariants
(business-logic-tester), Tier 3 validation oracle (analytic vs MC), Tier 4 golden
replay. Tests build and pass before any task is complete.

## 9. Build & increments

CMake (C++17): `monopoly_core` lib, `monopoly` REPL, `monopoly_tests`. Work is
decomposed into independently building, committable tasks (see the implementation
plan / tasks). Pre-commit hooks are not skipped.
