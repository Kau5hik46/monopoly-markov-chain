# Milestone 5: Configurable House Rules — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** A first-class, configurable rules subsystem: `RuleConfig` (toggles + params, JSON-loaded, fluent Builder) plus pure, testable rule-resolution helpers and Specifications for mugging, airport travel, and the Free-Parking house pot. The M6 executor orchestrates these into state mutations + effects.

**Architecture:** `monopoly::rules`. Pure logic only (no Effect/IO dependency) so it unit-tests in isolation: `RuleConfig` data + `RuleConfigBuilder`, `loadRuleConfig`, and free functions (`resolveMugging`, `freeParkingHousesForTax`, `isMuggingEligible`, `canTravelBetweenAirports`).

**Tech Stack:** C++17, nlohmann/json, `domain::GameState`, GoogleTest.

---

### Task 1: RuleConfig + Builder + loader

**Files:** Create `src/rules/rule_config.h`, `src/rules/rule_config.cpp`, `data/rules.default.json`, `tests/unit/test_rule_config.cpp`; modify `CMakeLists.txt`.

- [ ] `src/rules/rule_config.h`: `RuleConfig` struct (mugging, airport, free-parking,
  jail, pass-GO fields) + `RuleConfigBuilder` (fluent) + `RuleConfig loadRuleConfig(path)`.
- [ ] `data/rules.default.json`: all rules on, canonical params.
- [ ] `rule_config.cpp`: JSON parse with field-by-field defaults.
- [ ] Tests: builder sets fields; loader reads file; defaults when keys absent.

### Task 2: Pure rule helpers + Specifications

**Files:** Create `src/rules/rule_logic.h`, `src/rules/rule_logic.cpp`, `tests/unit/test_rule_logic.cpp`; modify `CMakeLists.txt`.

- [ ] `resolveMugging(muggerSum, muggeeSum)` → `MuggingResult` (ties → muggee escapes).
- [ ] `freeParkingHousesForTax(SquareType, RuleConfig)` → house count (income=2, super=1).
- [ ] `isMuggingEligible(SquareType)` → not Jail/FreeParking.
- [ ] `canTravelBetweenAirports(gs, player, from, to)` → both owned by player, both
  airports, distinct.
- [ ] Tests for each.

## Milestone 5 acceptance
- `ctest` green; rules are data-driven and pure-logic unit-tested. Files < 400 LoC.
