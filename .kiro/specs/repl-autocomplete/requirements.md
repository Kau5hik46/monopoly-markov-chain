# Requirements — REPL TAB Autocomplete (all commands + recommended actions)

Date: 2026-06-05
Status: Approved (brainstorming complete)

Extends the M9 `@`-square TAB completion (`engine/line_reader.cpp`) into full
command-aware completion, with first-class support for completing the **recommended
follow-up actions** the REPL already prints after each command (`CommandResult.prompts`).

## R1 — Recommended-action cycling (headline feature)
- **R1.1** After each command the REPL prints suggested follow-up actions (the
  `prompts`). These are captured and offered as TAB completions on the next line.
- **R1.2** On an **empty/fresh** input line, each TAB **cycles** to the next recommended
  action, replacing the line buffer with that action's **clean command form** (e.g. the
  prompt `"buy P1 @#1   (Old Kent Road, £600K)  — or skip"` completes to `buy P1 @#1`).
- **R1.3** Cycling wraps around; **Shift-TAB** cycles backward; **Esc** clears the line
  and exits cycling. Typing any character exits cycling and keeps what was typed.
- **R1.4** If there are no recommended actions, an empty-line TAB falls through to verb
  completion (R2).

## R2 — Command verb completion
On the **first token** (no space yet), TAB completes the command verb against the full
verb set (`init, roll, buy, sell, rent, build, mortgage, unmortgage, tax, jail, mug,
airport, claim, card, trade, cash, insure, write, settle, log, query, rules, save, load,
undo, help, quit`). Single match → fill; multiple → list + fill longest common prefix
(consistent with existing `@` behavior).

## R3 — Context-aware operand completion
After the verb, TAB completes operands by position/context:
- **R3.1** A token starting with `P`/`p` → player tokens `P1..PN` (N = live player count).
- **R3.2** After `query ` → query subcommands (`risk, options, dist, stationary, value,
  state, board, chain, forecast, simulate, ledger`).
- **R3.3** Otherwise → the keyword set valid in commands (`strike, premium, call, put,
  vs, on, off, all, income, landers, INCOME, SUPER, GO, JAIL, BACK3, STATION, UTILITY`),
  filtered by the current partial token.
- **R3.4** A partial token following `@` → square-name completion (existing M9 behavior,
  unchanged).

## R4 — Pure, testable completion core
The completion decision is a **pure function** over (buffer, completion model) returning
either a single replacement or a candidate list. The raw-terminal reader
(`line_reader.cpp`, requires a TTY) only does I/O + cycle bookkeeping and stays thin.
The prompt-cleaning function (R1.2) is independently unit-tested.

## R5 — No behavior change when non-interactive
Piped/non-TTY input is unaffected (the interactive reader is only used on a TTY, as
today). All existing tests stay green.

## Out of scope
History search, fuzzy matching, multi-line editing, mouse selection.
