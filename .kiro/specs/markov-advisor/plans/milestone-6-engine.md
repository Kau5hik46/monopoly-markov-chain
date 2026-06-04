# Milestone 6: Command DSL + REPL — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** The live advisor loop. A formal command DSL (lexer → parser → `Command`), an executor that mutates `GameState` and emits a causal `Effect` log (integrating the M5 rules), a formatter that prints effects + full state + advisory after every command, and a REPL that **asks the house rules at game start**.

**Architecture:** `monopoly::engine`. Pragmatic Interpreter/Command: the parser builds a `Command` value (kind + operands); the executor evaluates it (apply/undo via GameState snapshots = Memento). Queries delegate to the probability/pricing layers. Square refs: `@#<pos>` or `@<NORMALIZED_NAME>`. Amounts: int with optional `K`/`M` (decimals ok).

**Tech Stack:** C++17, all prior modules, GoogleTest.

---

## Files (each < 400 LoC)
- `engine/amount.h` — parse `K`/`M` amounts; format money in K/M.
- `engine/name_table.h/.cpp` — normalized square-name ↔ position.
- `engine/effect.h` — `Effect` (kind + text) + `CommandResult`.
- `engine/command.h` — `Command` (kind + operands).
- `engine/lexer.h/.cpp` — line → tokens.
- `engine/parser.h/.cpp` — tokens → `Command` (or parse error).
- `engine/executor.h/.cpp` — apply `Command` → mutate state + `Effect`s; undo stack.
- `engine/query_service.h/.cpp` — `query` commands via probability/pricing.
- `engine/formatter.h/.cpp` — render effects + state panel + advisory.
- `engine/repl.h/.cpp` — rules wizard + read/eval/print loop.
- `app/main.cpp` — wire REPL (default) / `--demo`.

## Commands (v1)
`init N` · `roll Pi = a,b` · `buy Pi @SQ [= amt]` · `sell Pi -> Pj @SQ [= amt]` ·
`rent Pi -> Pj @SQ [= amt]` · `build Pi @SQ +|- n` · `mortgage|unmortgage Pi @SQ` ·
`tax Pi = amt @INCOME|SUPER` · `jail Pi +|-` · `mug Pi vs Pj = a:b` ·
`airport Pi @FROM -> @TO` · `claim Pi` · `cash Pi +=|-= amt` ·
`query risk|options|dist|stationary|value|state [Pi|@SQ|^n]` · `rules <name> on|off` ·
`undo` · `help` · `quit`.

## Rules wizard (start of game)
On REPL start (interactive, unless `--rules-file F` or non-tty): prompt yes/no for
mugging, airport travel, free-parking pot, and pass-GO bonus, building a `RuleConfig`
via `RuleConfigBuilder`. Echo the chosen config.

## Tasks
1. `amount.h` + `name_table` + tests.
2. `effect.h` + `command.h` + `lexer` + `parser` + tests (parse each command form).
3. `executor` + `query_service` + tests (state mutation, effects, undo, queries).
4. `formatter` + `repl` (+ rules wizard) + golden-ish tests on formatter output.
5. Wire `main` (REPL default), manual smoke.

## Acceptance
- `ctest` green; a scripted session (piped stdin) drives the REPL end-to-end and prints
  effects + state + advisory; rules are prompted at start. Files < 400 LoC.
