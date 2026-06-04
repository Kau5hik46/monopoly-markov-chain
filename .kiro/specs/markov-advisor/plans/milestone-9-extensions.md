# Milestone 9: Extensions — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Three designed-for extensions:
1. **Monopoly-aware free-parking placement** — `claim` places pot houses onto the
   claimer's monopoly streets (even-building); unplaceable houses convert to cash.
2. **`card` / `trade` DSL verbs** — apply a drawn card's movement; swap property+cash bundles.
3. **N-roll + ruin forecast (req B/D)** — Monte-Carlo cumulative rent over N rolls and
   ruin probability (P(cumulative rent > cash within N)); `query forecast Pi ^N`.

**Tech Stack:** C++17, existing modules, GoogleTest.

## Feature 1 — free-parking placement
`rules::claimFreeParkingPot(GameState&, player) -> {placed, cashed}`: repeatedly place a
house on the eligible street with the fewest houses (street in a fully-owned, unmortgaged
group, houses < 5) until the pot empties or none are eligible; leftover houses return to
the bank and convert to cash. Executor `Claim` uses it.

## Feature 2 — card / trade
- `card Pi : GO|JAIL|BACK3|STATION|UTILITY | @SQ` — move accordingly (pass-GO bonus when
  wrapping), then resolve landing.
- `trade Pi <-> Pj : <bundleA> <-> <bundleB>` — each bundle = `@SQ ...` and an optional
  amount; transfer A's squares+cash to Pj and B's to Pi.
- Add `CommandKind::Card/Trade`, fields (`name`, `squaresA/B`, `amountA/B`), parser, executor.

## Feature 3 — N-roll / ruin forecast
`risk::forecast(gs, mover, N, trials, seed) -> {expectedCumulativeRent, ruinProbability}`:
simulate N rolls from the mover's position (dice + doubles/jail + sampled card-aware
landing via LandingResolver), accumulating `rentOwed` each roll; ruin = cumulative rent
exceeds the mover's cash within N. `query forecast Pi ^N` renders it.

## Tasks
1. Free-parking placement + tests.
2. card/trade parser+executor + tests.
3. forecast + query + tests.

## Acceptance
- `ctest` green; all three usable from the REPL. Files < 400 LoC.
