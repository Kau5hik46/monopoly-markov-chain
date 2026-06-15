# M11b — Income Options (turn-aware, multi-player) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (inline) or subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Add call/put options on an owner `Py`'s **income** — the total rent `Py` collects over the turn-aware window (until `Py`'s next turn), accounting for any opponent who might land on a `Py`-owned square. Builds on M11a.

**Architecture:** New pricing fn convolves each lander's next-roll rent-to-`Py` distribution into a total-income distribution (reuses `LossOutcome` + `fairValueAtStrike`). `OptionBook` gains income open + accumulation + turn-aware maturity. Executor accumulates rent to `Py` during the round and matures the contract when it's `Py`'s turn to roll again (the existing no-forget gate then applies). Income state (`landers`, `landersRolled`, `realizedValue`) already exists on `OptionContract` from M11a.

**Tech Stack:** C++17, GoogleTest. Build: `cmake --build build -j`; test: `ctest --test-dir build -R <Regex> --output-on-failure`.

Spec: `.kiro/specs/options-trading/{requirements.md (R8), design.md (Phase M11b)}`.

---

## Task 1: Income distribution + auto-lander detection (pricing)

**Files:** Modify `src/pricing/option_chain.{h,cpp}`; Test `tests/unit/test_income_chain.cpp`; Modify `CMakeLists.txt`.

Add to `option_chain.h`:
```cpp
// Opponents (≠ owner, not in jail) with > 0 probability of paying `owner` rent next roll.
std::vector<int> reachableLanders(const domain::GameState& gs, int owner,
                                  const probability::LandingResolver& resolver);

// Distribution of TOTAL rent `owner` collects when each lander takes one next roll,
// formed by convolving each lander's per-roll rent-to-owner distribution. rent>0 only
// (the no-income mass is implied by 1 - Σprob, as in buildLossDistribution).
std::vector<LossOutcome> buildIncomeDistribution(
    const domain::GameState& gs, int owner, const std::vector<int>& landers,
    const probability::LandingResolver& resolver);
```

Implementation notes (`option_chain.cpp`):
- Per-lander map<long,double> `value->prob` over 36 outcomes × card branches: for each resolved landing `lp` (skip jail sentinel), if `gs.ownerOf(lp.position) == owner && owner != lander && !gs.isMortgaged(lp.position)`, add `rentOwed(gs, lp.position, lander, sum)` with weight `(1/36)*lp.prob`; the remaining weight accrues to value 0.
- Convolve: start `{0:1.0}`; for each lander, `next[v1+v2] += p1*p2`.
- Emit `{prob, (double)value}` for `value > 0` only.
- `reachableLanders`: for each opponent, if its per-lander map has any `value>0` bucket, include it.

Tests:
- Single lander on a hotel square: income distribution equals that lander's own liability distribution to the owner (same `buildLossDistribution` restricted to owner squares).
- Two landers both able to hit `Py`: `E[income]` (fairValueAtStrike at K=0) equals the **sum** of each lander's `E[rent to Py]` (linearity of expectation through convolution).
- `reachableLanders` excludes a far-away opponent (place one 20 squares away) and the owner.
- Empty landers → empty distribution, `E=0`.

Commit: `feat(pricing): multi-lander income distribution via convolution`.

---

## Task 2: OptionBook income open + accumulation + turn-aware maturity

**Files:** Modify `src/contracts/option_book.{h,cpp}`; Test `tests/unit/test_option_book.cpp` (append).

Add to `option_book.h`:
```cpp
// Peer-written income option on owner `owner`. landers empty => auto-detect reachable.
// escrow = conservative bound Σ_landers 2 × maxRentToOwner(lander).
OpenResult openIncomePeer(domain::GameState& gs, int writer, int holder, int owner,
                          domain::OptionType type, long strike, long premium,
                          std::vector<int> landers,
                          const probability::LandingResolver& resolver);

// Bank-written income option (premium = fair value).
OpenResult openIncomeBank(domain::GameState& gs, int holder, int owner,
                          domain::OptionType type, long strike,
                          std::vector<int> landers,
                          const probability::LandingResolver& resolver);

// During the window: if `payer` is a referenced lander of an open income contract on
// `owner`, add `rent` to its realizedValue and bump landersRolled.
void accumulateIncome(domain::GameState& gs, int owner, int payer, long rent);

// When `owner` is about to take their turn again, mature their open income contracts
// that have seen at least one lander roll (turn-aware window closed).
void matureIncomeOnOwnerTurn(domain::GameState& gs, int owner);
```

Implementation:
- A shared private `openIncome(...)` does validation + landers resolution (`reachableLanders` if empty) + escrow bound (`2 × max rent to owner` per lander, summed) + premium move + escrow lock (peer) and stores `underlying=Income, insured=owner, landers`.
- Escrow bound helper: for each lander, the max single rent it could pay `owner` over reachable squares (max over `buildIncomeDistribution` of a single lander), times 2; summed.
- `accumulateIncome`: for each `Open`, `Income` contract with `insured==owner` and `payer ∈ landers`: `realizedValue += rent; ++landersRolled`.
- `matureIncomeOnOwnerTurn`: for each `Open`, `Income` contract with `insured==owner && landersRolled>0`: set `Matured`.
- `settle`/`payoffOf`/`firstMaturedSummary` already type-branch and use `realizedValue` (M11a) — no change needed; payout caps at escrow.

Tests (append, fixture funds players to £50M):
- Income peer open locks escrow > 0, premium moves; landers auto-detected non-empty.
- Accumulate from two landers then `matureIncomeOnOwnerTurn` → realizedValue = sum; settle call pays `max(income−K,0)` capped at escrow.
- Income put pays when income stays below K.
- No-default: escrow ≥ any realized income (mature with a huge value, payout capped at escrow).

Commit: `feat(contracts): income option open, accumulation, turn-aware maturity`.

---

## Task 3: DSL — income clause + landers

**Files:** Modify `src/engine/command.h`, `src/engine/parser.cpp`; Test `tests/unit/test_options_dsl.cpp` (append).

`command.h`: add `bool isIncome = false; std::vector<int> landers;` to `Command`.

`parser.cpp`, inside `insure` and `write` after the call/put keyword: if `cur.eat("income")` → `c.isIncome = true;` then **require** a player → `c.insured = owner`. (For income the owner is explicit; do not default to holder.) For `write`, after strike/premium, allow trailing `landers P.. P..`: while next token is a player, push to `c.landers`. (Place `landers` parse before `premium`? — choose: `write ... income Py strike K premium P landers Pa Pb`. Parse `landers` last.)

Liability path unchanged (no `income` keyword → insured defaults as today).

Tests:
- `insure P1 call income P2 strike 1M` → isIncome, insured=1 (P2), strike set.
- `write P2 -> P1 call income P3 strike 2M premium 1M landers P3 P4` → isIncome, insured=2, landers={2,3}.
- Existing liability parse tests still pass.

Commit: `feat(engine): parse income option clause and landers list`.

---

## Task 4: Executor wiring — accumulation + income maturity + gate ordering

**Files:** Modify `src/engine/executor.{h,cpp}`; Test `tests/unit/test_options_exec.cpp` (append).

- Add member `int lastRentOwner_ = -1;` set alongside `lastRentPaid_` in `resolveLanding` (the owner who received the rent).
- **Income maturity (before the gate):** right after `snapshot()` and before the gate, for a `Roll` by `c.player`, call `contracts::matureIncomeOnOwnerTurn(gs_, c.player)` so an income option on the roller matures as their turn comes up; the gate then blocks the roll until settled.
- **Accumulation (post-roll):** in the post-switch hook, when `r.ok && c.kind==Roll && lastRentPaid_>0 && lastRentOwner_>=0`, call `contracts::accumulateIncome(gs_, lastRentOwner_, c.player, lastRentPaid_)`.
- **Open cases:** in `Insure`/`Write`, branch on `c.isIncome`: call `openIncomeBank`/`openIncomePeer` with `c.insured` as owner and `c.landers`; else the existing liability path.
- Reset `lastRentOwner_ = -1;` at the start of the `Roll` case (next to `lastRentPaid_ = 0;`).

Tests (Harness funds players; give P2 a hotel; P1 & P3 positioned to reach it):
- Open income call on P2 via `write`; P1 rolls onto P2's hotel and pays rent → accumulated (not yet matured). Then it becomes P2's turn → `roll P2 = ...` is **blocked** (income matured, gate). `settle all` pays the holder.
- Income put end-to-end.
- Liability options unaffected (existing tests pass).

Commit: `feat(engine): income option accumulation, turn-aware maturity, open wiring`.

---

## Task 5: Full suite + help + roadmap

**Files:** `src/engine/help.cpp` (income syntax line); `.kiro/specs/markov-advisor/tasks.md` (M11b → done); full `ctest`.

- Help: extend the OPTIONS section with the `income Py [landers ...]` forms.
- Run full `ctest` — all green.
- Manual smoke: piped session opening an income option, rolling opponents onto the owner's hotel, owner's-turn gate, settle, `query ledger`.
- Roadmap row M11b → ✅ done.

Commit: `feat(engine): income option help; docs: roadmap M11b done`.

---

## Self-Review
- R8.1 turn-aware income underlying → T1 (convolution) + T4 (accumulate to owner's next turn).
- R8.2 auto-detect + override → T1 `reachableLanders` + T3 `landers`.
- R8.3 incremental maturity, gate → T2 `matureIncomeOnOwnerTurn` + T4 ordering before gate.
- R8.4 conservative escrow, payout cap → T2 escrow bound + M11a `settle` cap.
- R8.5 fair value → T1 `fairValueAtStrike` on income dist.
- Known simplification: accumulation counts rent from referenced landers during the window; card-advance rent and >1-roll-per-lander doubles are approximated (documented). Escrow's 2× bound covers the common doubles case; payout cap guarantees no-default regardless.
