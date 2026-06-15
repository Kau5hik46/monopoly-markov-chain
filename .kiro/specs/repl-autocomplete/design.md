# Design — REPL TAB Autocomplete

Date: 2026-06-05
Status: Approved (brainstorming complete)

## Overview
Split completion into a **pure decision core** (`engine/completion.{h,cpp}`, unit-tested
without a TTY) and the existing **raw-terminal reader** (`engine/line_reader.cpp`, thin
I/O + cycle bookkeeping). The REPL feeds the prior command's recommended actions into
the model each iteration.

## Components

### `engine/completion.h` — model + pure API
```cpp
namespace monopoly::engine {

struct CompletionModel {
  const NameTable* names = nullptr;          // for @square completion
  int numPlayers = 0;                         // for P1..PN
  std::vector<std::string> recommendedActions;  // clean command forms (R1)
  // Static tables (filled by a factory): verbs, querySubs, keywords.
  std::vector<std::string> verbs;
  std::vector<std::string> querySubs;
  std::vector<std::string> keywords;
};

struct CompletionResult {
  bool replaced = false;            // buffer should be replaced by `replacement`
  std::string replacement;          // new buffer (single match / common-prefix / cycle)
  std::vector<std::string> candidates;  // to list above the prompt (size != 1)
};

// Strip a recommended-action prompt to its executable command form (R1.2):
// cut at the first "  (" or " — " / " -- " / "  —"; trim trailing spaces.
std::string cleanPrompt(const std::string& prompt);

// Build the static-table portion of the model (verbs/querySubs/keywords).
CompletionModel makeCompletionModel(const NameTable& names, int numPlayers,
                                    std::vector<std::string> recommendedActions);

// Pure completion. `cycleIndex` is used only for empty-line recommended-action cycling
// (R1.2/R1.3); callers pass the running TAB count. Returns the action at
// recommendedActions[cycleIndex % size] when buf is empty and actions exist.
CompletionResult complete(const std::string& buf, const CompletionModel& model,
                          int cycleIndex);

}  // namespace monopoly::engine
```

### `complete()` decision order
1. **`@` token** — if the text after the last `@` is the active token (no space after it),
   delegate to `names->completions(prefix)` → single fill / list + common prefix (R3.4).
2. **Empty buffer** — if `recommendedActions` non-empty, return
   `{replaced=true, replacement = recommendedActions[cycleIndex % N]}` (R1.2). Else fall
   to verb completion with empty prefix (lists all verbs).
3. **First token (no space in buf)** — complete against `verbs` (R2).
4. **After `query` as first token** — complete `querySubs` (R3.2).
5. **Otherwise** — candidate pool = `{P1..PN} ∪ keywords`, filtered by the current
   partial token (R3.1/R3.3). Single → fill; many → list + common prefix.

Single-match and common-prefix filling reuse the same logic the current `complete()` has
for `@` (longest-common-prefix helper moves into `completion.cpp`).

### `engine/line_reader.cpp` — thin reader changes
- `readInteractiveLine` signature gains the model:
  `bool readInteractiveLine(const CompletionModel& model, const std::string& prompt,
   std::string& out, std::ostream& term);` (drops the bare `NameTable&`; the model holds
   `names`). Update the REPL call site.
- Maintain `int tabCycles = 0;` in the read loop. On TAB: call `complete(buf, model,
  tabCycles)`; if `replaced`, set `buf`, `++tabCycles`, redraw; if candidates listed,
  print them then redraw; reset `tabCycles = 0` on any non-TAB key.
- **Shift-TAB**: the terminal sends `ESC [ Z`. The existing escape-swallow branch must be
  extended to detect `[ Z` and decrement the cycle (cycle backward) instead of swallowing.
- **Esc** (lone `ESC` with no sequence): clear `buf`, exit cycling.

### `engine/repl.cpp` — feed recommended actions
- Keep the previous command's `CommandResult` (the REPL already renders it). Before each
  `readInteractiveLine`, build `recommendedActions` = `map(cleanPrompt, lastResult.prompts)`
  and `makeCompletionModel(names_, gs_.numPlayers(), actions)`.
- First iteration (no prior result) → empty recommendedActions (verb completion only).

## Verb / keyword tables (single source of truth)
`makeCompletionModel` hard-codes:
- verbs: `init roll buy sell rent build mortgage unmortgage tax jail mug airport claim
  card trade cash insure write settle log query rules save load undo help quit`
- querySubs: `risk options dist stationary value state board chain forecast simulate ledger`
- keywords: `strike premium call put vs on off all income landers INCOME SUPER GO JAIL
  BACK3 STATION UTILITY`

> These mirror `parser.cpp`. A code comment in both files cross-references the other so
> they stay in sync (a new verb must be added to both).

## Testing (`tests/unit/test_completion.cpp`)
Pure, no TTY:
- `cleanPrompt` strips `"  (...)"` and `" — ..."` tails; leaves clean commands intact.
- Empty buffer + recommendedActions → cycles through actions by `cycleIndex`.
- Empty buffer + no actions → lists all verbs.
- `"in"` → completes `init`/`insure` common prefix `in`, lists both.
- `"insu"` → single match fills `insure`.
- `"query "` → lists query subcommands; `"query le"` → fills `ledger`.
- `"buy P"` → lists `P1..PN`.
- `"write P2 -> P1 st"` → fills `strike` (keyword).
- `"buy P1 @old"` → delegates to NameTable square completion.

The raw-terminal `line_reader` is exercised only via existing manual/REPL smoke paths
(TTY-dependent); its logic is kept minimal by delegating decisions to `complete()`.

## Files
New: `src/engine/completion.{h,cpp}`, `tests/unit/test_completion.cpp`.
Edited: `src/engine/line_reader.{h,cpp}` (use model + cycling + Shift-TAB/Esc),
`src/engine/repl.cpp` (build model from last result), `CMakeLists.txt`.
Each file < 400 LoC. No change to non-interactive behavior.
