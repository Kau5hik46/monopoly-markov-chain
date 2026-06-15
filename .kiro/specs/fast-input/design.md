# Design — Fast Real-Time Input

Date: 2026-06-15
Status: Implemented

## Overview
A thin input layer sits between the raw-mode reader and the parser. All recognition is a
pure function (`engine/fastpath`); the REPL applies it, the parser/executor/boards are
untouched. Rolls and recommended actions become 1–3 keystrokes.

## Pure core (`engine/fastpath.{h,cpp}`)
- `buildMenu(prompts)` → `vector<MenuAction>`: cleans each prompt (`cleanPrompt`), **drops
  roll prompts** (dice shorthand is their fast path), and classifies `skippable`
  (prompt contains "skip"/"optional"), `moneyMoving` (verb ∈ buy/sell/trade/tax/write/
  insure), `hasPlaceholder` (`<…>`).
- `defaultActionIndex(menu)` → first non-skippable, else `-1` (Enter skips).
- `interpret(line, menu, currentRoller)` → `FastInput{None|Run|Prefill, text}`:
  - `d,d` / `d d` (1..6) → `Run "roll P{currentRoller+1} = d,d"`.
  - blank → default action; `-1` ⇒ `None` (skip).
  - lone digit `N` in range → action `N`.
  - else `None` (parser handles the line).
  - Run vs Prefill: placeholder ⇒ Prefill truncated at `<` (e.g. `mug P1 vs P2 = `);
    money-moving ⇒ Prefill full command (confirm with Enter); otherwise ⇒ Run.

## Reader (`engine/line_reader`)
`readInteractiveLine(..., const std::string& initial = "")` — the buffer starts at
`initial`, enabling accept-to-confirm pre-fill and error-line recovery. No other change;
TAB/cycling/Shift-TAB/Esc unchanged.

## REPL (`engine/repl.cpp`)
Per iteration: build the menu from the previous command's prompts; read with
`pendingFill` as the initial buffer; `interpret` the line.
- `Prefill` (interactive) → `pendingFill = text; continue` (loads the buffer to confirm/
  complete). Piped → falls through to running `text`.
- `Run`/`None` → feed the (expanded or raw) line to the parser.
- After execution: on failure, `pendingFill = original` (G1 error recovery); refresh the
  completion actions and print a one-line **fast menu**:
  `fast: [↵] settle all  [2] mug P1 vs P2 = …` (default bold, rest dim).

## Why no parser/executor change
The REPL expands shorthand/accepts into ordinary command strings (`roll P1 = 3,4`,
`buy P2 @#1`) before `parseLine`, so the grammar, turn-order checks, options, and both
boards are unaffected.

## Files
New: `src/engine/fastpath.{h,cpp}`, `tests/unit/test_fastpath.cpp`, this spec.
Edited: `src/engine/line_reader.{h,cpp}` (initial buffer), `src/engine/repl.cpp`
(expansion + menu + recovery), `CMakeLists.txt`.

## Tests (`tests/unit/test_fastpath.cpp`, 9)
Dice shorthand (current player, range/separator rejection); menu build excludes rolls and
classifies; default = first mandatory else skip; Enter skips all-optional / runs mandatory;
digit accepts money action as prefill; placeholder prefill up to `<`; normal lines pass
through. Verified end-to-end: `3,4`→P1 rolls, `5 2`→P2 rolls (turn-aware); landing on a
buyable then `1` runs `buy …`. Full suite 138/138.

## Deferred
Single-letter verb aliases; mid-line cursor editing; multi-command lines; reordering the
tax grammar so the `@TYPE` suffix survives placeholder pre-fill (today `tax` pre-fills to
`tax Pi = ` and the operator re-adds `@INCOME`).
