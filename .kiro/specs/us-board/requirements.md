# Requirements — US Board & Board Selection

Date: 2026-06-15
Status: Implemented

Adds a second board edition (standard US Monopoly) selectable at startup, with values in
**literal dollars** ($60 Mediterranean Avenue), alongside the existing London board which
is **left unchanged** (canonical UK values ×10⁴, £).

## R1 — US board data (literal dollars)
- **R1.1** `data/board.us.json` — 40 squares with canonical US names and **literal**
  prices/rents (Mediterranean $60, Boardwalk $400, rents $2…$2000). Currency `"$"`.
- **R1.2** `rentRules` in literal dollars: station rents `[25,50,100,200]`, utility pips
  `4`/`10`.
- **R1.3** `data/decks.us.json` — US Chance/Community-Chest movement set (targets map to
  the US positions: GO=0, St. Charles=11, Illinois=24, Reading=5, Boardwalk=39).
- **R1.4** `data/board.us.csv` — human title-deed reference matching the JSON.

## R2 — Board-dependent money units
The money scale is a property of the board edition, not a global constant.
- **R2.1** Display currency comes from the board (`Board::currency()`, JSON `currency`,
  default `£`). `formatMoney` renders the active symbol (`engine::moneySymbol()`), set at
  startup from the chosen board.
- **R2.2** Economy values (`startingCash`, `passGoBonus`, `jailFine`, `muggingAmount`,
  `freeParkingHouseCash`) live in `RuleConfig` and are seeded per edition from
  `data/rules.<edition>.json`. London = ×10⁴ (`startingCash` £15M, salary £2M); US =
  literal ($1500 start, $200 salary, $50 jail fine).
- **R2.3** The London edition's behavior and numbers are **unchanged**.

## R3 — Board selection
- **R3.1** CLI flag `--board <london|us>` (aliases uk/usa), default chosen via prompt.
- **R3.2** When `--board` is absent and stdin is a TTY, prompt at startup
  (`[1] London (£)  [2] US ($)`). Non-interactive defaults to London.
- **R3.3** `--demo` never prompts (honors `--board` or defaults to London).
- **R3.4** The chosen edition loads `board.<edition>.json` + `decks.<edition>.json` and
  seeds the wizard from `rules.<edition>.json`.

## R4 — Tests
US board loads with literal values (Mediterranean price 60, station rent 25, currency $);
US rules load literal economy ($1500/$200/$50); `formatMoney` honors the active symbol;
London board still defaults to £. Full suite stays green.

## Out of scope
A CSV-driven board loader (CSV is a reference only); US-specific Chance/CC card *text*
(movement targets only, matching the engine's card model).
