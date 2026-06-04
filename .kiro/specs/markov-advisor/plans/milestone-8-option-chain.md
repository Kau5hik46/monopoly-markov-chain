# Milestone 8: Strike-Based Option Chain — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Requirement C. Treat the next roll's rent liability as a random variable `L`
and price insurance across a ladder of strikes/deductibles `K`: fair premium =
`E[max(L − K, 0)]`, with `P(L > K)` (payout probability). Expose `query chain Pi`.

**Architecture:** `monopoly::pricing::option_chain` builds the discrete loss
distribution from the same 36-outcome × card-branch enumeration the pricer uses, then
evaluates a nicely-rounded strike ladder. `query_service` renders it.

**Tech Stack:** C++17, existing pricing/probability, GoogleTest.

## Math
- Loss distribution: for each dice outcome (1/36) and resolved landing (card-aware),
  `L = rentOwed(landing, arrivalSum)` (0 if no rent). Collect `(prob, rent)` pairs.
- For strike `K`: `premium(K) = Σ prob·max(rent − K, 0)`, `payoutProb(K) = Σ prob[rent>K]`.
- `premium(0) = E[L]` (equals the existing expectedRent — a sanity check).
- Strike ladder: 0, then a "nice" step (1/2/5×10^k) up to `maxLoss`.

## Tasks
1. `src/pricing/option_chain.h/.cpp` + `tests/unit/test_option_chain.cpp`:
   `buildLossDistribution`, `buildOptionChain`. Tests: premium(0)==E[L], premium
   monotone non-increasing in K, payoutProb non-increasing, premium→0 at K≥maxLoss.
2. DSL: `QueryKind::Chain`, parse `query chain Pi`, render a strike table in
   `query_service`; add a `help` line. Wire CMake.

## Acceptance
- `ctest` green; `query chain Pi` prints a strike ladder with payout prob + premium.
  Files < 400 LoC.
