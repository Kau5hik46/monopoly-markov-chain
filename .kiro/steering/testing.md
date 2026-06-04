# Testing Steering — Monopoly Markov Advisor

## Framework
GoogleTest, run via CTest. Tests live in `tests/` and are split into four tiers.

## Tier 1 — Unit
- **DiceModel**: joint `P(sum=k, isDouble)` table sums to 1; doubles mass is 6/36;
  mugging contest `P(mugger>muggee) ≈ 0.4437`, `P(muggee≥) ≈ 0.5563`.
- **math**: matrix multiply, power iteration convergence, linear solve correctness
  against hand-checked small systems.
- **Parser (Interpreter)**: every DSL production parses to the right Command;
  malformed lines produce structured parse errors, never crashes.
- **RentTable**: rent resolution for street (site/1–4/hotel), station (count-based),
  utility (dice-multiplier), mortgage (0), monopoly site-doubling.
- **Rule handlers**: Mugging, AirportTravel, FreeParkingPot, JailPolicy each tested
  in isolation with crafted states.

## Tier 2 — Invariants (business-logic-tester owns these)
- Stationary `π`: all entries ≥ 0, sums to 1.
- **Cash conservation**: total cash across players + bank changes only by explicit
  bank injections/taxes; transfers net to zero.
- **House supply**: houses in play + hotels (×... ) + pot ≤ bank supply (32/12);
  free-parking pot conserves houses↔cash on claim.
- **Ledger balances**: every CashTransfer effect has matching debit/credit.
- **Mugging coupling**: exactly one of {muggee→Free Parking, mugger→Jail} occurs per
  contest; never both, never neither.

## Tier 3 — Validation oracle
- With **all coupling rules off**, the Monte-Carlo landing frequencies must match the
  analytic stationary `π` within tolerance (e.g. total variation < 0.005 at large K).
- Landing-frequency sanity: Jail is the most-occupied square; the orange/red-analog
  group shows the expected post-jail elevation; airports/utilities ordering matches
  the known Monopoly result adapted to this board.
- Single-roll `L(s)` from a known position integrates to 1 and matches an independent
  brute-force enumeration of 2d6 + card branching.

## Tier 4 — Golden replay
- Scripted `.session` DSL files replayed end-to-end; assert on the ordered `Effect`
  list (causal side-effect chain) and on the rendered game-state panel (snapshot).
- At least one golden session per house rule exercising its full cascade
  (e.g. doubles → mugging → hospital → £500K transfer → roll again).

## Discipline
- No converting implementation to TODO, including in tests.
- Tests must build and pass before a task is considered complete.
- Pre-commit hooks must not be skipped.
