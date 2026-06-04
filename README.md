# Monopoly Markov Advisor

A C++17 decision-support tool for a **live, in-progress** game of (London-edition)
Monopoly. You type real game events as they happen; after each one the advisor
re-renders the full board state and tells you the **roll risk for the next player** and
a **fair insurance price for the upcoming roll** — backed by an exact Markov-chain model
of the board and a Monte-Carlo simulator that cross-validates it.

It is not an autonomous player. It is a co-pilot: it tracks ground truth from your input
and answers *"given where things actually are, what is the risk, and what's the fair
price of insuring the next roll?"*

```
ADVISORY  next-to-roll: P1 ----------------------------------------- RISK: LOW
 Expected rent liability ........ £114.6K
 Max single-roll risk ........... £11.00M  on TRAFALGAR SQUARE
 Mugging expected value ......... +£2.7K  (benefit)
 Fair insurance premium ......... £111.9K

 TOP THREATS                       LAND%   EXP
  1. TRAFALGAR SQUARE (24)        1.041%  £114.6K
```

## Features

- **Position-only Markov chain** over ~123 micro-states (square × consecutive-doubles +
  in-jail attempts) with full Chance/Community-Chest **card branching**, the
  3-doubles→jail rule, and a correct **in-jail vs just-visiting** distinction. Produces
  exact **stationary** and **N-step transient** landing distributions.
- **Monte-Carlo backend** that mirrors the chain and validates it (stationary matches
  the analytic result within total-variation tolerance), plus a **multi-player
  rule-coupled** simulator for effects the analytic model can't express (e.g. mugging
  displacement).
- **Risk & insurance pricing** (requirements A–D):
  - single-roll expected rent liability, max single-roll risk, and a **top-threats** list;
  - a **strike-based option chain** — fair premium `E[max(loss − K, 0)]` across deductibles;
  - **N-roll cumulative rent** and **ruin probability** forecasts.
- **Configurable house rules**, asked at game start: mugging (with a 2d6 contest,
  victim → Free Parking / mugger → Jail), airport travel between owned airports,
  the Free-Parking **house pot** (taxes become parked houses, claimed onto monopolies),
  2× salary for landing exactly on GO, and jail policy.
- **Live REPL** with a formal command DSL, turn-order enforcement, an **ACTION NEEDED**
  prompt for expected follow-ups, `@`-name **tab autocomplete**, colourised sectioned
  output (auto-off when piped), man-page `help`, `undo`, and **save/load** to disk.
- **Real UK Monopoly rents** (×10,000 to match the board's price scale); all board,
  card-deck, and rule data live in JSON config under `data/`.

## Build & run

Requires a C++17 compiler and CMake ≥ 3.16. GoogleTest and nlohmann/json are fetched
automatically.

```bash
cmake -S . -B build
cmake --build build -j
./build/monopoly            # interactive advisor (colour + autocomplete in a terminal)
./build/monopoly --demo     # quick long-run landing-probability showcase
./build/monopoly --color    # force ANSI colour even when piped
ctest --test-dir build      # run the test suite
```

The data directory is resolved from `argv[1]`, then `$MONOPOLY_DATA_DIR`, then a
compile-time default.

## Command language

One verb per line. Players are `P1, P2, …`; squares are `@#<pos>` or
`@<NAME_WITH_UNDERSCORES>` (TAB-complets); amounts take `K`/`M` suffixes.

| Command | Meaning |
|---|---|
| `init N` | start a game with N players |
| `roll Pi = d1,d2` | apply a dice roll (GO bonus, jail, doubles handled; turn order enforced) |
| `buy Pi @SQ [= amt]` / `sell Pi -> Pj @SQ [= amt]` | buy / transfer a property |
| `rent Pi -> Pj @SQ [= amt]` | pay rent (auto-computed if omitted) |
| `build Pi @SQ +\|- n` · `mortgage\|unmortgage Pi @SQ` | develop / mortgage |
| `tax Pi = amt @INCOME\|SUPER` · `cash Pi +=\|-= amt` | money movement |
| `jail Pi +\|-` · `mug Pi vs Pj = a:b` · `airport Pi @FROM -> @TO` | jail / mugging / travel |
| `card Pi : GO\|JAIL\|BACK3\|STATION\|UTILITY\|@SQ\|+amt\|-amt` | apply a drawn card |
| `trade Pi <-> Pj : <bundle> <-> <bundle>` | swap property + cash bundles |
| `claim Pi` | claim the Free-Parking house pot |
| `query state\|board\|risk Pi\|options Pi\|chain Pi\|dist Pi ^n\|forecast Pi ^n\|simulate ^n\|stationary\|value @SQ` | analysis |
| `rules <mugging\|airport\|pot> on\|off` · `save FILE` · `load FILE` · `undo` · `help` · `quit` | control |

`help` prints a full man-page.

## Architecture

Acyclic layers: `math ← probability ← risk ← pricing`, `domain ← rules`,
`engine ← app`.

```
src/
  math/         dense Matrix + power-iteration stationary solver
  domain/       Board, Square, ColorGroup, decks, GameState (live ground truth)
  rules/        configurable RuleConfig + pure rule logic
  probability/  dice, 123-state transition matrix, analytic + Monte-Carlo backends
  risk/         rent table, single-roll risk, N-roll/ruin forecast, coupled game sim
  pricing/      single-roll insurance pricer, strike-based option chain
  engine/       DSL lexer/parser, command executor (+undo), query service, formatter,
                REPL (+ rules wizard, autocomplete), session save/load
  app/          entry point
data/           board.london.json, decks.london.json, rules.default.json
tests/          GoogleTest unit / invariant / validation / golden tiers
```

Design is documented under [`.kiro/`](.kiro/) (steering, spec, and milestone plans).

## The model in brief

Each die-throw is one step of a Markov chain over `(square, doubles)` plus dedicated
in-jail states. Card squares branch into their deck's movement outcomes (including
chained Back-3 → Community-Chest re-draws). The **stationary** distribution gives long-run
landing frequencies (Jail dominates, the orange/red group is elevated by the go-to-jail
and back-3 cards — the canonical Monopoly result); the **transient** distribution from a
known position gives the next-N-roll odds. Money never enters the chain — the risk layer
overlays live ownership/cash/houses to turn probabilities into expected rent, option
premiums, and ruin estimates. Coupling rules (mugging displacing players) are handled by
the multi-player Monte-Carlo simulator.

## License

[MIT](LICENSE) © 2026 Kaushik.
