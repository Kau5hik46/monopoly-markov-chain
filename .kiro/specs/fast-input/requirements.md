# Requirements — Fast Real-Time Input

Date: 2026-06-15
Status: Approved (UX design complete)

Cuts the 3–10s per-event entry cost by removing redundant input, building on the existing
parser, recommended-action prompts, and TAB engine. No change to game rules, pricing,
boards (London/US), or the 129 passing tests.

## R1 — Turn-aware roll shorthand
A bare dice entry rolls for the current player (`nextRoller`):
- **R1.1** `3,4` or `3 4` → `roll P{nextRoller} = 3,4`. The full form `roll Pi = d,d`
  still works.
- **R1.2** Parser emits a roll with player unset (`-1`); the executor fills `nextRoller_`.
- **R1.3** A **lone digit** is never a valid roll, so it is free to mean "accept action N"
  (R2) — the two never collide.

## R2 — Accept recommended actions (one-key + Enter default)
After each command the follow-up prompts render as a ranked menu with a default.
- **R2.1** Blank `Enter` accepts the **default** action; digit `N` accepts action `N`.
- **R2.2 Default selection (forgiveness):** the default is the first **mandatory** action
  (settle/mug/tax/card after landing); if all actions are **optional** (buy / airport /
  claim — prompts containing "skip"/"optional"), the default is **skip** (Enter does
  nothing). Enter never spends money by accident.
- **R2.3 Hybrid accept:** a chosen action runs immediately when it is unambiguous and
  non-money (roll/settle/claim/skip); it is **pre-filled into the buffer** for a
  confirming Enter when it moves money (buy/sell/trade/tax/write/insure) or still has a
  `<placeholder>`.
- **R2.4** Roll prompts are excluded from the menu (the R1 shorthand is their fast path).

## R3 — Extra gaps
- **R3.1 (G1) Error-line preservation:** a rejected line (parse error or failed command)
  is pre-loaded into the next buffer so the operator edits instead of retyping.
- **R3.2 (G2) One-key skip:** for optional prompts the default Enter dismisses them
  (no command), so non-events cost a single keystroke.
- **R3.3 (G3) Mug/tax pre-fill:** accepting a placeholder prompt loads it up to the first
  `<…>` (e.g. `mug P1 vs P2 = `) so only the numbers are typed.

## R4 — Scope & safety
- **R4.1** All recognition logic is a **pure function** (`interpret`/`buildMenu`),
  unit-tested without a TTY; the raw-mode reader only gains an initial-buffer parameter.
- **R4.2** Non-interactive (piped) input is unaffected for normal commands; shorthand and
  accept-by-number still expand, but pre-fill (which needs editing) degrades to running a
  complete command or is skipped for placeholder commands.

## Out of scope
Single-letter verb aliases (deferred), mid-line cursor editing, multi-command lines.
