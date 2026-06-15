# Design — US Board & Board Selection

Date: 2026-06-15
Status: Implemented

## Overview
A board edition is fully described by three data files — `board.<ed>.json`,
`decks.<ed>.json`, `rules.<ed>.json` — and the money scale is whatever those files use.
London uses ×10⁴ (unchanged); US uses literal dollars. The engine is scale-agnostic: it
manipulates `long` amounts and renders them with a per-board currency symbol.

## Currency (display)
- `Board` gains `currency_` (default `"£"`, UTF-8), loaded from the optional JSON
  `currency` field (`board_factory.cpp`).
- `engine::moneySymbol()` is a process-global string (in `amount.h`); `formatMoney` emits
  it instead of a hard-coded `£`. `main.cpp` sets `moneySymbol() = board.currency()` right
  after loading the chosen board. Tests restore the default after overriding.

## Economy (values) — board-dependent units
- `RuleConfig` gains `startingCash` (default 15000000) and `freeParkingHouseCash`
  (default 1000000); `loadRuleConfig` parses both. The executor uses
  `rules_.startingCash` / `rules_.freeParkingHouseCash` (the old `kStartingCash` /
  `kFreeParkingHouseCash` constants remain as the default seed values).
- `runRulesWizard(in, out, base)` seeds numeric economy from `base` and toggles only the
  booleans. `main.cpp` loads `rules.<edition>.json` as `base` (falling back to code
  defaults if the file is absent).

## Selection (`main.cpp`)
- `flagValue(args, "--board")` reads the edition; `chooseBoard` normalizes aliases
  (uk→london, usa→us), prompts interactively on a TTY when no flag is given, and defaults
  to London non-interactively. `--demo` skips the prompt.
- `resolveDataDir` skips the token following `--board` so the board name isn't mistaken
  for the data directory.
- Loads `board.<ed>.json`, `decks.<ed>.json`; sets the currency symbol; seeds rules.

## Data
- `data/board.us.json` — 40 literal-dollar squares, `currency:"$"`, literal `rentRules`.
- `data/decks.us.json` — US movement targets (same positions as the London analogs).
- `data/rules.us.json` — literal economy; `data/rules.london.json` — the ×10⁴ economy
  (mirrors the historical code defaults so London is byte-for-byte unchanged in behavior).
- `data/board.us.csv` — title-deed reference (POSITION,NAME,TYPE,GROUP,PRICE,HOUSE_COST,
  MORTGAGE,RENT_SITE…RENT_HOTEL).

## Files
New: `data/board.us.{json,csv}`, `data/decks.us.json`, `data/rules.{us,london}.json`,
`tests/unit/test_currency.cpp`, this spec.
Edited: `src/domain/board.{h}` (+`board_factory.cpp` currency), `src/engine/amount.h`
(moneySymbol), `src/rules/rule_config.{h,cpp}` (startingCash/freeParkingHouseCash),
`src/engine/executor.cpp` (use rules_ economy), `src/engine/repl.{h,cpp}` (wizard base),
`src/app/main.cpp` (selection), `CMakeLists.txt` (US test paths).

## Tests (`tests/unit/test_currency.cpp`)
`Currency.*` (default £, formatMoney symbol), `UsBoard.LiteralDollarValuesAndCurrency`
(Mediterranean 60, station 25, `$`), `UsBoard.EconomyIsLiteralDollars` ($1500/$200/$50).
Verified end-to-end: `monopoly --board us` shows "buys MEDITERRANEAN AVENUE for $60",
starting cash $1.5K; London still shows £600.0K / £15.00M.
