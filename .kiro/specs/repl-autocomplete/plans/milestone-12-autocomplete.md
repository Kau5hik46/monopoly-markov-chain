# M12 — REPL TAB Autocomplete — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the M9 `@`-square TAB completion into full command-aware completion: cycle the on-screen recommended actions on an empty line, complete command verbs, and complete context operands (players, keywords, query subcommands).

**Architecture:** A pure decision core `engine/completion.{h,cpp}` (unit-tested without a TTY) does all completion logic; the raw-terminal `engine/line_reader.cpp` stays thin (I/O + cycle bookkeeping); `engine/repl.cpp` feeds the previous command's recommended actions into the model.

**Tech Stack:** C++17, GoogleTest, raw-mode termios (existing).

**Build/test:** `cmake --build build -j` ; `ctest --test-dir build -R <Regex> --output-on-failure`.

Spec: `.kiro/specs/repl-autocomplete/{requirements,design}.md`. Depends only on existing
`engine/name_table` and the REPL; **independent of options trading** (works whether or not
M11a is built — the `insure/write/settle/log/ledger` tokens are just strings in the verb
and keyword tables).

---

## Task 1: Pure completion core — model, cleanPrompt, complete()

**Files:**
- Create: `src/engine/completion.h`, `src/engine/completion.cpp`
- Test: `tests/unit/test_completion.cpp`
- Modify: `CMakeLists.txt` (add `src/engine/completion.cpp` to `monopoly_core`; add the test file)

- [ ] **Step 1: Write the header**

```cpp
// src/engine/completion.h
#pragma once
#include <string>
#include <vector>
#include "engine/name_table.h"

namespace monopoly::engine {

struct CompletionModel {
  const NameTable* names = nullptr;
  int numPlayers = 0;
  std::vector<std::string> recommendedActions;  // clean command forms
  std::vector<std::string> verbs;
  std::vector<std::string> querySubs;
  std::vector<std::string> keywords;
};

struct CompletionResult {
  bool replaced = false;
  std::string replacement;
  std::vector<std::string> candidates;
};

// Strip a recommended-action prompt to its executable command form.
std::string cleanPrompt(const std::string& prompt);

// Build a model with the static verb/querySub/keyword tables filled in.
CompletionModel makeCompletionModel(const NameTable& names, int numPlayers,
                                    std::vector<std::string> recommendedActions);

// Pure completion decision. `cycleIndex` selects the recommended action on an empty line.
CompletionResult complete(const std::string& buf, const CompletionModel& model,
                          int cycleIndex);

}  // namespace monopoly::engine
```

- [ ] **Step 2: Write the failing test and register it in CMake**

Add `src/engine/completion.cpp` to `add_library(monopoly_core …)` and
`tests/unit/test_completion.cpp` to `add_executable(monopoly_tests …)`.

```cpp
// tests/unit/test_completion.cpp
#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/completion.h"
#include "engine/name_table.h"

using namespace monopoly::engine;
using monopoly::domain::loadBoardFromFile;

namespace {
CompletionModel model(int players, std::vector<std::string> actions, const NameTable& nt) {
  return makeCompletionModel(nt, players, std::move(actions));
}
}  // namespace

TEST(Completion, CleanPromptStripsTails) {
  EXPECT_EQ(cleanPrompt("buy P1 @#1   (Old Kent Road, \xC2\xA3""600K)  \xE2\x80\x94 or skip"),
            "buy P1 @#1");
  EXPECT_EQ(cleanPrompt("settle all"), "settle all");
  EXPECT_EQ(cleanPrompt("tax P1 = <amount> @INCOME"), "tax P1 = <amount> @INCOME");
  EXPECT_EQ(cleanPrompt("claim P1   (free-parking pot: 2 houses)"), "claim P1");
}

TEST(Completion, EmptyBufferCyclesRecommendedActions) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {"buy P1 @#1", "settle all"}, nt);
  auto a = complete("", m, 0);
  ASSERT_TRUE(a.replaced); EXPECT_EQ(a.replacement, "buy P1 @#1");
  auto b = complete("", m, 1);
  ASSERT_TRUE(b.replaced); EXPECT_EQ(b.replacement, "settle all");
  auto c = complete("", m, 2);  // wraps
  ASSERT_TRUE(c.replaced); EXPECT_EQ(c.replacement, "buy P1 @#1");
}

TEST(Completion, EmptyBufferNoActionsListsVerbs) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {}, nt);
  auto r = complete("", m, 0);
  EXPECT_FALSE(r.replaced);
  EXPECT_FALSE(r.candidates.empty());
}

TEST(Completion, VerbPrefixCompletes) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {}, nt);
  auto single = complete("insu", m, 0);
  ASSERT_TRUE(single.replaced); EXPECT_EQ(single.replacement, "insure");
  auto multi = complete("in", m, 0);   // init, insure
  EXPECT_FALSE(multi.replaced);
  EXPECT_GE(multi.candidates.size(), 2u);
}

TEST(Completion, QuerySubcommandsAndOperands) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(3, {}, nt);
  auto led = complete("query le", m, 0);
  ASSERT_TRUE(led.replaced); EXPECT_EQ(led.replacement, "query ledger");
  auto players = complete("buy P", m, 0);
  EXPECT_EQ(players.candidates.size(), 3u);   // P1 P2 P3
  auto kw = complete("write P2 -> P1 st", m, 0);
  ASSERT_TRUE(kw.replaced); EXPECT_EQ(kw.replacement, "write P2 -> P1 strike");
}

TEST(Completion, AtSquareDelegatesToNameTable) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {}, nt);
  auto r = complete("buy P1 @Old", m, 0);   // depends on board having an "Old ..." square
  // Either a single fill or a candidate list, but it must have engaged @ completion:
  EXPECT_TRUE(r.replaced || !r.candidates.empty());
}
```

> Note: `\xC2\xA3` is `£`, `\xE2\x80\x94` is the em dash `—` — these are the exact bytes
> the REPL prompts use. The `AtSquare…` test assumes a square name beginning "Old"; if the
> London board uses different names, change the prefix to one that exists (see
> `data/board.london.json`).

- [ ] **Step 3: Run, verify it fails to link/compile**

Run: `cmake --build build -j 2>&1 | head` — Expected: `completion.h`/symbols missing.

- [ ] **Step 4: Implement `completion.cpp`**

```cpp
// src/engine/completion.cpp
#include "engine/completion.h"

#include <algorithm>

namespace monopoly::engine {

namespace {

std::string commonPrefix(const std::vector<std::string>& v) {
  if (v.empty()) return "";
  std::string p = v.front();
  for (const auto& s : v) {
    std::size_t k = 0;
    while (k < p.size() && k < s.size() && p[k] == s[k]) ++k;
    p.resize(k);
  }
  return p;
}

std::vector<std::string> filterByPrefix(const std::vector<std::string>& pool,
                                        const std::string& pfx) {
  std::vector<std::string> out;
  for (const auto& s : pool)
    if (s.size() >= pfx.size() && s.compare(0, pfx.size(), pfx) == 0) out.push_back(s);
  return out;
}

// Fill `r` from `pool` against the current token; `head` is everything before the token.
CompletionResult fillFrom(const std::vector<std::string>& pool, const std::string& token,
                          const std::string& head) {
  CompletionResult r;
  auto matches = filterByPrefix(pool, token);
  if (matches.size() == 1) { r.replaced = true; r.replacement = head + matches[0]; return r; }
  if (!matches.empty()) {
    const std::string lcp = commonPrefix(matches);
    if (lcp.size() > token.size()) { r.replaced = true; r.replacement = head + lcp; }
    r.candidates = matches;
  }
  return r;
}

std::string rstrip(std::string s) {
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

}  // namespace

std::string cleanPrompt(const std::string& prompt) {
  std::size_t cut = std::string::npos;
  for (const char* marker : {"  (", " \xE2\x80\x94", "  \xE2\x80\x94", " --"}) {
    auto idx = prompt.find(marker);
    if (idx != std::string::npos) cut = std::min(cut, idx);
  }
  return rstrip(cut == std::string::npos ? prompt : prompt.substr(0, cut));
}

CompletionModel makeCompletionModel(const NameTable& names, int numPlayers,
                                    std::vector<std::string> recommendedActions) {
  CompletionModel m;
  m.names = &names;
  m.numPlayers = numPlayers;
  m.recommendedActions = std::move(recommendedActions);
  m.verbs = {"init", "roll", "buy", "sell", "rent", "build", "mortgage", "unmortgage",
             "tax", "jail", "mug", "airport", "claim", "card", "trade", "cash", "insure",
             "write", "settle", "log", "query", "rules", "save", "load", "undo", "help",
             "quit"};
  m.querySubs = {"risk", "options", "dist", "stationary", "value", "state", "board",
                 "chain", "forecast", "simulate", "ledger"};
  m.keywords = {"strike", "premium", "call", "put", "vs", "on", "off", "all", "income",
                "landers", "INCOME", "SUPER", "GO", "JAIL", "BACK3", "STATION", "UTILITY"};
  return m;
}

CompletionResult complete(const std::string& buf, const CompletionModel& model,
                          int cycleIndex) {
  CompletionResult r;

  // 1. Active @square token.
  const auto at = buf.rfind('@');
  const auto sp = buf.rfind(' ');
  const bool atActive = at != std::string::npos && (sp == std::string::npos || at > sp);
  if (atActive && model.names) {
    const std::string prefix = buf.substr(at + 1);
    auto matches = model.names->completions(prefix);
    if (matches.size() == 1) {
      r.replaced = true; r.replacement = buf.substr(0, at + 1) + matches[0]; return r;
    }
    if (!matches.empty()) {
      const std::string lcp = commonPrefix(matches);
      if (lcp.size() > prefix.size()) {
        r.replaced = true; r.replacement = buf.substr(0, at + 1) + lcp;
      }
      r.candidates = matches;
    }
    return r;
  }

  // 2. Empty buffer: cycle recommended actions, else list verbs.
  if (buf.empty()) {
    const int n = static_cast<int>(model.recommendedActions.size());
    if (n > 0) {
      const int i = ((cycleIndex % n) + n) % n;
      r.replaced = true;
      r.replacement = model.recommendedActions[static_cast<std::size_t>(i)];
      return r;
    }
    r.candidates = model.verbs;
    return r;
  }

  const std::string token = (sp == std::string::npos) ? buf : buf.substr(sp + 1);
  const std::string head = (sp == std::string::npos) ? "" : buf.substr(0, sp + 1);

  // 3. First token -> verbs.
  if (sp == std::string::npos) return fillFrom(model.verbs, token, head);

  // 4. After "query" first token -> query subcommands.
  const std::string first = buf.substr(0, buf.find(' '));
  if (first == "query") return fillFrom(model.querySubs, token, head);

  // 5. Otherwise players ∪ keywords.
  std::vector<std::string> pool;
  for (int i = 1; i <= model.numPlayers; ++i) pool.push_back("P" + std::to_string(i));
  pool.insert(pool.end(), model.keywords.begin(), model.keywords.end());
  return fillFrom(pool, token, head);
}

}  // namespace monopoly::engine
```

- [ ] **Step 5: Build and run, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R Completion --output-on-failure`
Expected: all PASS. (Adjust the `@Old` prefix in the last test if needed per the board data.)

- [ ] **Step 6: Commit**

```bash
git add src/engine/completion.h src/engine/completion.cpp tests/unit/test_completion.cpp CMakeLists.txt
git commit -m "feat(engine): pure command-aware completion core (verbs, operands, recommended actions)"
```

---

## Task 2: Wire the completion core into the raw-terminal reader

**Files:**
- Modify: `src/engine/line_reader.h`, `src/engine/line_reader.cpp`

- [ ] **Step 1: Change the signature in `line_reader.h`**

```cpp
#pragma once
#include <ostream>
#include <string>
#include "engine/completion.h"

namespace monopoly::engine {

// Reads one line in raw mode with TAB completion driven by `model` (verbs, operands,
// @square, and empty-line recommended-action cycling). Returns false on EOF.
bool readInteractiveLine(const CompletionModel& model, const std::string& prompt,
                         std::string& out, std::ostream& term);

}  // namespace monopoly::engine
```

- [ ] **Step 2: Rewrite the TAB / escape handling in `line_reader.cpp`**

Replace the old `complete(...)` free function and its call with calls into the core, and
add cycle bookkeeping + Shift-TAB/Esc. Keep `redraw`. The reader becomes:

```cpp
#include "engine/line_reader.h"

#include <termios.h>
#include <unistd.h>

#include <string>
#include "engine/completion.h"

namespace monopoly::engine {

namespace {
void redraw(std::ostream& term, const std::string& prompt, const std::string& buf) {
  term << "\r\x1b[K" << prompt << buf;
  term.flush();
}

// Apply a CompletionResult to the terminal + buffer.
void applyCompletion(const CompletionResult& cr, std::string& buf, std::ostream& term,
                     const std::string& prompt) {
  if (cr.replaced) { buf = cr.replacement; redraw(term, prompt, buf); return; }
  if (cr.candidates.empty()) { term << "\a"; term.flush(); return; }
  term << "\r\n";
  std::size_t shown = 0;
  for (const auto& m : cr.candidates) {
    if (shown++ >= 24) { term << "  ... (" << (cr.candidates.size() - 24) << " more)"; break; }
    term << "  " << m;
  }
  term << "\r\n";
  redraw(term, prompt, buf);
}
}  // namespace

bool readInteractiveLine(const CompletionModel& model, const std::string& prompt,
                         std::string& out, std::ostream& term) {
  termios oldt;
  if (tcgetattr(STDIN_FILENO, &oldt) != 0) return false;
  termios raw = oldt;
  raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON | ECHO | ISIG));
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  std::string buf;
  int tabCycles = 0;
  redraw(term, prompt, buf);
  bool eof = false;
  for (;;) {
    char ch;
    const ssize_t n = ::read(STDIN_FILENO, &ch, 1);
    if (n <= 0) { eof = buf.empty(); break; }
    if (ch == '\n' || ch == '\r') { term << "\r\n"; term.flush(); break; }
    if (ch == 4) { eof = buf.empty(); if (eof) break; continue; }   // Ctrl-D
    if (ch == 3) { buf.clear(); tabCycles = 0; term << "^C\r\n"; redraw(term, prompt, buf); continue; }
    if (ch == 127 || ch == 8) {
      if (!buf.empty()) { buf.pop_back(); }
      tabCycles = 0; redraw(term, prompt, buf); continue;
    }
    if (ch == '\t') {
      applyCompletion(complete(buf, model, tabCycles), buf, term, prompt);
      ++tabCycles;
      continue;
    }
    if (ch == 27) {  // ESC: could be a lone Esc, or CSI sequence (arrows, Shift-TAB).
      char seq0;
      const ssize_t m0 = ::read(STDIN_FILENO, &seq0, 1);
      if (m0 <= 0) {                 // lone Esc: clear line, exit cycling
        buf.clear(); tabCycles = 0; redraw(term, prompt, buf); continue;
      }
      char seq1; ::read(STDIN_FILENO, &seq1, 1);
      if (seq0 == '[' && seq1 == 'Z') {   // Shift-TAB: cycle backward
        --tabCycles;
        applyCompletion(complete(buf, model, tabCycles), buf, term, prompt);
        continue;
      }
      continue;  // swallow other sequences (arrow keys, etc.)
    }
    if (static_cast<unsigned char>(ch) >= 32) {
      buf.push_back(ch);
      tabCycles = 0;
      term << ch;
      term.flush();
    }
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  if (eof) return false;
  out = buf;
  return true;
}

}  // namespace monopoly::engine
```

> Behavior change for Shift-TAB: previously the reader read a fixed 2 bytes after ESC;
> now it reads the first byte, treats a failed read as a lone Esc (clear line), and reads
> the second byte only for CSI. This is functionally compatible with the old arrow-key
> swallow. If the platform delivers Esc and the next byte non-atomically, the lone-Esc
> path relies on `VMIN=1` blocking — acceptable for this interactive tool.

- [ ] **Step 3: Build (no unit test — TTY path)**

Run: `cmake --build build -j 2>&1 | tail`
Expected: compiles. (The REPL call site is updated in Task 3; until then the build may
fail at `repl.cpp` — that is expected and fixed next.)

- [ ] **Step 4: Commit (after Task 3 builds clean — see note)**

Defer the commit to the end of Task 3 so the tree compiles. (Tasks 2 and 3 form one
buildable unit.)

---

## Task 3: Feed recommended actions from the REPL

**Files:**
- Modify: `src/engine/repl.cpp`

- [ ] **Step 1: Find the read call site**

In `repl.cpp`, the loop currently calls `readInteractiveLine(names_, prompt, line, out_)`
(for the interactive TTY branch). Locate it and the place where the previous
`CommandResult` is available (the loop renders it via `render(...)`).

- [ ] **Step 2: Build the model from the last result and pass it**

Add includes: `#include "engine/completion.h"` and `#include <algorithm>`.

Keep the last result's prompts across iterations. Minimal change: store
`std::vector<std::string> lastActions_;` as a local in `run(...)` (not a member), updated
each iteration:

```cpp
  std::vector<std::string> lastActions;  // declared before the loop
  // ... inside the loop, when reading interactively:
  CompletionModel model = makeCompletionModel(names_, gs_.numPlayers(), lastActions);
  if (!readInteractiveLine(model, prompt, line, out_)) break;  // EOF
  // ... after executing the command and obtaining `result`:
  lastActions.clear();
  for (const auto& p : result.prompts) lastActions.push_back(cleanPrompt(p));
```

> Match the actual variable names in `repl.cpp` (`result`, `line`, `prompt`, `out_`). The
> non-interactive (piped) branch using `std::getline` is unchanged — completion only
> applies to the TTY reader.

- [ ] **Step 3: Build and run the full suite, verify green**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: all tests pass (no test exercises the TTY path; `test_completion` covers logic).

- [ ] **Step 4: Manual smoke (interactive)**

Run: `./build/bin/monopoly` (or the built REPL path). Type `init 2`, press Enter, then on
the fresh line press TAB repeatedly — it should cycle the recommended follow-up actions.
Type `in` + TAB → completes toward `init`/`insure`. Type `query le` + TAB → `query ledger`.
(Manual check; not automated.)

- [ ] **Step 5: Commit (Tasks 2 + 3 together)**

```bash
git add src/engine/line_reader.h src/engine/line_reader.cpp src/engine/repl.cpp
git commit -m "feat(engine): cycle recommended actions + verb/operand TAB completion in REPL"
```

---

## Task 4: Roadmap + docs

**Files:**
- Modify: `.kiro/specs/markov-advisor/tasks.md`

- [ ] **Step 1: Add a roadmap row**

```
| **M12** | **REPL autocomplete** | ✅ done | command-aware TAB completion: verbs, context operands (players/keywords/query subs), @square, and empty-line cycling of recommended actions | `.kiro/specs/repl-autocomplete/plans/milestone-12-autocomplete.md` | M9 |
```

- [ ] **Step 2: Commit**

```bash
git add .kiro/specs/markov-advisor/tasks.md
git commit -m "docs: roadmap M12 REPL autocomplete"
```

---

## Self-Review (completed by plan author)

**Spec coverage:** R1 recommended-action cycling → Task 1 (`complete` empty-buffer branch
+ `cleanPrompt`) + Task 2 (cycle bookkeeping) + Task 3 (feed from REPL). R2 verb completion
→ Task 1 branch 3. R3 context operands → Task 1 branches 4/5 + `@` branch 1. R4 pure core →
Task 1 (all logic in `completion.cpp`, tested without TTY). R5 non-interactive unchanged →
Task 3 leaves the `getline` branch alone.

**Placeholder scan:** No TBD/TODO; all code complete. Three verify-against-existing notes
(REPL call-site variable names in Task 3; the `@Old` square prefix in the Task 1 test;
Shift-TAB byte handling in Task 2) are explicit integration checks with stated resolutions.

**Type consistency:** `CompletionModel`/`CompletionResult`/`complete`/`cleanPrompt`/
`makeCompletionModel` defined in Task 1 and used verbatim in Tasks 2/3. The verb/keyword
tables live solely in `makeCompletionModel` (single source of truth; cross-referenced to
`parser.cpp` per the design).

**Sync risk:** adding a new command verb later requires updating both `parser.cpp` and
`makeCompletionModel` — noted via the design's cross-reference comment requirement.
