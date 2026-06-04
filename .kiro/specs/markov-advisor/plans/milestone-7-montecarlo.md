# Milestone 7: Monte-Carlo Backend & Validation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** A seeded position-only Monte-Carlo simulator implementing `IDistributionProvider`, mirroring the analytic chain dynamics (dice, doubles→jail, jail policy, stochastic card draws). It serves as the validation oracle: with rules off, MC's empirical stationary distribution must match the analytic `π` within tolerance.

**Architecture:** `monopoly::probability::MonteCarloEngine` (Bridge sibling of `ProbabilityEngine`). Injected seed for reproducibility. Single long walk → empirical stationary + jail; M independent trials → single-roll / N-roll distributions.

> **Scope note:** multi-player rule-coupled simulation (mugging displacement etc.) is a
> documented future extension; M7 delivers the position-only oracle that proves the
> analytic engine correct, which is the highest-value validation.

**Tech Stack:** C++17, `<random>` (`std::mt19937`), GoogleTest.

## Tasks
1. `monte_carlo.h/.cpp` — `MonteCarloEngine`, mirroring the per-roll chain + stochastic
   card draws; implement the four `IDistributionProvider` methods.
2. `test_monte_carlo.cpp` — MC stationary ≈ analytic `π` (total variation < 0.02);
   MC single-roll peak matches analytic; sums sane. Wire into CMake.

## Acceptance
- `ctest` green; analytic vs MC agreement within tolerance. Files < 400 LoC.
