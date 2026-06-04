# Milestone 3: Analytic Probability Engine — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Build the position-only Markov chain over ~123 micro-states (square × doubles-streak + in-jail attempts) with full card branching and the 3-doubles→jail rule, then expose stationary, single-roll, and N-roll transient landing distributions. Wire a demo into `main`.

**Architecture:** `monopoly::probability`. `MicroState` indexing → `LandingResolver` (card/go-to-jail redirection with chained draws) → `TransitionMatrix` (123×123 via `math::Matrix`) → `ProbabilityEngine` facade. `IDistributionProvider` (Bridge) lets later backends (Monte-Carlo, M7) plug in behind the same interface.

**Tech Stack:** C++17, `monopoly::math::Matrix`/`stationaryDistribution`, GoogleTest.

---

## Micro-state model (the math)

State indices (123 total):
- On-board `(pos, d)` for `pos ∈ 0..39`, `d ∈ {0,1,2}` (consecutive doubles this turn):
  `index = pos*3 + d` → 0..119.
- In-jail `(attempts a)` for `a ∈ {0,1,2}`: `index = 120 + a` → 120..122.

Per-roll chain (one step = one 2d6 throw). From `(pos, d)`, enumerate 36 outcomes (1/36 each):
- Double **and** `d == 2` (3rd double): → in-jail `(0)`. Turn ends.
- Otherwise move to `(pos+sum) % 40`, then **resolve landing** (below). If resolution
  yields jail → in-jail `(0)`. Else destination `dst`:
  - double (and `d<2`): → `(dst, d+1)` (player rolls again);
  - non-double: → `(dst, 0)`.

From in-jail `(a)`, throw 2d6:
- double: leave jail, move to `(10+sum)%40`, resolve, → `(dst, 0)` (no extra roll);
- non-double and `a<2`: → in-jail `(a+1)`;
- non-double and `a==2`: pay fine, move to `(10+sum)%40`, resolve, → `(dst, 0)`.

**Resolve landing** `resolveLanding(pos)` returns a distribution over destinations
(`-1` = jail sentinel):
- `pos == 30` (GO_TO_JAIL): `{-1: 1}`.
- Chance/Community-Chest square: for each card effect (prob `weight/16`):
  `None` → stay at `pos` (terminal, no redraw); `AdvanceTo t` → recurse `resolveLanding(t)`;
  `GoToJail` → `-1`; `NearestStation`/`NearestUtility` → recurse on `board.nearestForward`;
  `Back3` → recurse `resolveLanding((pos-3+40)%40)`. Recursion handles e.g. Back-3 from
  square 36 landing on Community Chest 33 (a genuine second draw). Depth-guarded.
- otherwise: `{pos: 1}`.

The resulting 123×123 matrix is row-stochastic. Stationary `π` marginalizes to a
40-vector (sum over `d`) plus a separate jail probability.

---

### Task 1: MicroState indexing

**Files:** Create `src/probability/micro_state.h`; create `tests/unit/test_microstate.cpp`; modify `CMakeLists.txt`.

- [ ] **Step 1: Implement `src/probability/micro_state.h`**

```cpp
#pragma once
#include <cstddef>

namespace monopoly::probability {

inline constexpr int kNumPositions = 40;
inline constexpr int kMaxDoubles = 3;            // d in {0,1,2}
inline constexpr int kOnBoardStates = kNumPositions * kMaxDoubles;  // 120
inline constexpr int kJailStates = 3;            // attempts a in {0,1,2}
inline constexpr int kNumStates = kOnBoardStates + kJailStates;     // 123
inline constexpr int kJailSentinel = -1;         // resolveLanding "go to jail"

inline int onBoardIndex(int pos, int doubles) { return pos * kMaxDoubles + doubles; }
inline int jailIndex(int attempts) { return kOnBoardStates + attempts; }
inline bool isJailState(int idx) { return idx >= kOnBoardStates; }
inline int positionOf(int onBoardIdx) { return onBoardIdx / kMaxDoubles; }
inline int doublesOf(int onBoardIdx) { return onBoardIdx % kMaxDoubles; }

}  // namespace monopoly::probability
```

- [ ] **Step 2: Tests** (`tests/unit/test_microstate.cpp`), add file to `monopoly_tests`.

```cpp
#include <gtest/gtest.h>
#include "probability/micro_state.h"
using namespace monopoly::probability;

TEST(MicroState, IndexRoundTrip) {
  EXPECT_EQ(kNumStates, 123);
  int idx = onBoardIndex(24, 1);
  EXPECT_EQ(positionOf(idx), 24);
  EXPECT_EQ(doublesOf(idx), 1);
  EXPECT_FALSE(isJailState(idx));
  EXPECT_TRUE(isJailState(jailIndex(0)));
  EXPECT_EQ(jailIndex(2), 122);
}
```

- [ ] **Step 3:** Build + `ctest -R MicroState` → PASS. **Commit** `feat(probability): micro-state indexing`.

---

### Task 2: LandingResolver

**Files:** Create `src/probability/landing.h` / `landing.cpp`; create `tests/unit/test_landing.cpp`; modify `CMakeLists.txt`.

- [ ] **Step 1: Implement `src/probability/landing.h`**

```cpp
#pragma once
#include <vector>
#include "domain/board.h"
#include "domain/deck_factory.h"

namespace monopoly::probability {

// (position, probability); position == kJailSentinel (-1) means "go to jail".
struct LandingProb { int position; double prob; };

// Resolves a raw landing square into terminal destinations, applying
// GO_TO_JAIL redirection and Chance/Community-Chest card draws (with chained
// re-draws, e.g. Back-3 onto a Community Chest square).
class LandingResolver {
 public:
  LandingResolver(const domain::Board& board, const domain::Decks& decks)
      : board_(board), decks_(decks) {}

  std::vector<LandingProb> resolve(int rawPosition) const;

 private:
  void accumulate(int pos, double prob, int depth,
                  std::vector<double>& posMass, double& jailMass) const;
  const domain::Board& board_;
  const domain::Decks& decks_;
};

}  // namespace monopoly::probability
```

- [ ] **Step 2: Implement `src/probability/landing.cpp`**

```cpp
#include "probability/landing.h"

#include "domain/card.h"
#include "probability/micro_state.h"

namespace monopoly::probability {

using domain::CardMove;
using domain::SquareType;

void LandingResolver::accumulate(int pos, double prob, int depth,
                                 std::vector<double>& posMass,
                                 double& jailMass) const {
  if (depth > 8) {  // safety guard; real chains are shallow
    posMass[pos] += prob;
    return;
  }
  const auto& sq = board_.at(pos);
  if (sq.type == SquareType::GoToJail) {
    jailMass += prob;
    return;
  }
  const domain::CardDeck* deck = nullptr;
  if (sq.type == SquareType::Chance) deck = &decks_.chance;
  else if (sq.type == SquareType::CommunityChest) deck = &decks_.communityChest;

  if (deck == nullptr) {
    posMass[pos] += prob;  // terminal square
    return;
  }

  const double n = static_cast<double>(deck->size());
  for (const auto& e : deck->effects) {
    const double p = prob * (static_cast<double>(e.weight) / n);
    switch (e.move) {
      case CardMove::None:
        posMass[pos] += p;  // stay, no redraw
        break;
      case CardMove::GoToJail:
        jailMass += p;
        break;
      case CardMove::AdvanceTo:
        accumulate(e.target, p, depth + 1, posMass, jailMass);
        break;
      case CardMove::NearestStation:
        accumulate(board_.nearestForward(pos, SquareType::Station), p, depth + 1,
                   posMass, jailMass);
        break;
      case CardMove::NearestUtility:
        accumulate(board_.nearestForward(pos, SquareType::Utility), p, depth + 1,
                   posMass, jailMass);
        break;
      case CardMove::Back3:
        accumulate((pos - 3 + kNumPositions) % kNumPositions, p, depth + 1,
                   posMass, jailMass);
        break;
    }
  }
}

std::vector<LandingProb> LandingResolver::resolve(int rawPosition) const {
  std::vector<double> posMass(kNumPositions, 0.0);
  double jailMass = 0.0;
  accumulate(rawPosition % kNumPositions, 1.0, 0, posMass, jailMass);

  std::vector<LandingProb> out;
  for (int p = 0; p < kNumPositions; ++p)
    if (posMass[p] > 0.0) out.push_back({p, posMass[p]});
  if (jailMass > 0.0) out.push_back({kJailSentinel, jailMass});
  return out;
}

}  // namespace monopoly::probability
```

- [ ] **Step 3: Tests** (`tests/unit/test_landing.cpp`)

```cpp
#include <gtest/gtest.h>
#include <cmath>
#include "probability/landing.h"
#include "probability/micro_state.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"

using namespace monopoly::probability;
using namespace monopoly::domain;

namespace {
double total(const std::vector<LandingProb>& v) {
  double s = 0; for (auto& l : v) s += l.prob; return s;
}
double massAt(const std::vector<LandingProb>& v, int pos) {
  for (auto& l : v) if (l.position == pos) return l.prob; return 0.0;
}
}

TEST(Landing, PlainSquareIsTerminal) {
  auto b = loadBoardFromFile(BOARD_JSON_PATH);
  auto d = loadDecksFromFile(DECKS_JSON_PATH);
  LandingResolver r(b, d);
  auto out = r.resolve(1);  // Portobello Road Market
  EXPECT_NEAR(total(out), 1.0, 1e-12);
  EXPECT_NEAR(massAt(out, 1), 1.0, 1e-12);
}

TEST(Landing, GoToJailRedirects) {
  auto b = loadBoardFromFile(BOARD_JSON_PATH);
  auto d = loadDecksFromFile(DECKS_JSON_PATH);
  LandingResolver r(b, d);
  auto out = r.resolve(30);
  EXPECT_NEAR(massAt(out, kJailSentinel), 1.0, 1e-12);
}

TEST(Landing, ChanceDistributesAndConserves) {
  auto b = loadBoardFromFile(BOARD_JSON_PATH);
  auto d = loadDecksFromFile(DECKS_JSON_PATH);
  LandingResolver r(b, d);
  auto out = r.resolve(7);  // Chance
  EXPECT_NEAR(total(out), 1.0, 1e-12);
  // 6/16 "stay" mass remains on square 7.
  EXPECT_NEAR(massAt(out, 7), 6.0 / 16.0, 1e-12);
  // 1/16 advance to GO(0); 2/16 nearest station from 7 == 15.
  EXPECT_NEAR(massAt(out, 0), 1.0 / 16.0, 1e-12);
  EXPECT_GE(massAt(out, 15), 2.0 / 16.0 - 1e-12);
  // some jail mass from the go-to-jail card.
  EXPECT_GE(massAt(out, kJailSentinel), 1.0 / 16.0 - 1e-12);
}
```

- [ ] **Step 4:** Build + `ctest -R Landing` → PASS. **Commit** `feat(probability): landing resolver with card branching`.

---

### Task 3: TransitionMatrix

**Files:** Create `src/probability/transition_matrix.h` / `.cpp`; create `tests/unit/test_transition.cpp`; modify `CMakeLists.txt`.

- [ ] **Step 1: Implement `src/probability/transition_matrix.h`**

```cpp
#pragma once
#include "math/matrix.h"
#include "probability/landing.h"
#include "domain/board.h"
#include "domain/deck_factory.h"

namespace monopoly::probability {

// Builds the 123x123 row-stochastic per-roll transition matrix.
class TransitionMatrix {
 public:
  TransitionMatrix(const domain::Board& board, const domain::Decks& decks);
  const math::Matrix& matrix() const { return p_; }

 private:
  void addLanding(int fromState, int rawTarget, int nextDoubles, double weight,
                  bool toJail);
  math::Matrix p_;
  LandingResolver resolver_;
};

}  // namespace monopoly::probability
```

- [ ] **Step 2: Implement `src/probability/transition_matrix.cpp`**

```cpp
#include "probability/transition_matrix.h"

#include "probability/micro_state.h"

namespace monopoly::probability {

namespace {
constexpr double kPerOutcome = 1.0 / 36.0;
}

// Distributes `weight` from `fromState` across the resolved landings of moving to
// `rawTarget`; jail sentinel routes to in-jail(0); board squares carry `nextDoubles`.
void TransitionMatrix::addLanding(int fromState, int rawTarget, int nextDoubles,
                                  double weight, bool /*toJail*/) {
  for (const auto& lp : resolver_.resolve(rawTarget)) {
    if (lp.position == kJailSentinel)
      p_(fromState, jailIndex(0)) += weight * lp.prob;
    else
      p_(fromState, onBoardIndex(lp.position, nextDoubles)) += weight * lp.prob;
  }
}

TransitionMatrix::TransitionMatrix(const domain::Board& board,
                                   const domain::Decks& decks)
    : p_(kNumStates, kNumStates, 0.0), resolver_(board, decks) {
  // On-board states.
  for (int pos = 0; pos < kNumPositions; ++pos) {
    for (int d = 0; d < kMaxDoubles; ++d) {
      const int from = onBoardIndex(pos, d);
      for (int a = 1; a <= 6; ++a) {
        for (int b = 1; b <= 6; ++b) {
          const bool dbl = (a == b);
          const int sum = a + b;
          if (dbl && d == kMaxDoubles - 1) {
            p_(from, jailIndex(0)) += kPerOutcome;  // 3rd double -> jail
          } else {
            const int nextD = dbl ? d + 1 : 0;
            addLanding(from, (pos + sum) % kNumPositions, nextD, kPerOutcome, false);
          }
        }
      }
    }
  }
  // In-jail states.
  for (int a = 0; a < kJailStates; ++a) {
    const int from = jailIndex(a);
    for (int d1 = 1; d1 <= 6; ++d1) {
      for (int d2 = 1; d2 <= 6; ++d2) {
        const bool dbl = (d1 == d2);
        const int sum = d1 + d2;
        if (dbl) {
          addLanding(from, (10 + sum) % kNumPositions, 0, kPerOutcome, false);
        } else if (a < kJailStates - 1) {
          p_(from, jailIndex(a + 1)) += kPerOutcome;  // stay in jail
        } else {
          addLanding(from, (10 + sum) % kNumPositions, 0, kPerOutcome, false);  // pay & move
        }
      }
    }
  }
}

}  // namespace monopoly::probability
```

- [ ] **Step 3: Tests** (`tests/unit/test_transition.cpp`) — every row sums to 1.

```cpp
#include <gtest/gtest.h>
#include <cmath>
#include "probability/transition_matrix.h"
#include "probability/micro_state.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"

using namespace monopoly::probability;

TEST(Transition, EveryRowIsStochastic) {
  auto b = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto d = monopoly::domain::loadDecksFromFile(DECKS_JSON_PATH);
  TransitionMatrix tm(b, d);
  const auto& p = tm.matrix();
  ASSERT_EQ(p.rows(), static_cast<std::size_t>(kNumStates));
  for (std::size_t i = 0; i < p.rows(); ++i) {
    double s = 0.0;
    for (std::size_t j = 0; j < p.cols(); ++j) s += p(i, j);
    EXPECT_NEAR(s, 1.0, 1e-9) << "row " << i;
  }
}
```

- [ ] **Step 4:** Build + `ctest -R Transition` → PASS. **Commit** `feat(probability): 123-state transition matrix`.

---

### Task 4: ProbabilityEngine facade + IDistributionProvider

**Files:** Create `src/probability/distribution_provider.h`; `src/probability/probability_engine.h` / `.cpp`; `tests/unit/test_probability_engine.cpp`; modify `CMakeLists.txt`.

- [ ] **Step 1: Implement `src/probability/distribution_provider.h`** (Bridge interface)

```cpp
#pragma once
#include <array>
#include "probability/micro_state.h"

namespace monopoly::probability {

using PositionVector = std::array<double, kNumPositions>;

// Common interface so analytic and (later) Monte-Carlo backends are interchangeable.
class IDistributionProvider {
 public:
  virtual ~IDistributionProvider() = default;
  // Long-run landing probability per board position (jail mass reported separately).
  virtual PositionVector stationaryByPosition() const = 0;
  virtual double jailProbability() const = 0;
  // Distribution over landing squares after exactly one throw from `fromPos` (d=0).
  virtual PositionVector singleRoll(int fromPos) const = 0;
  // Distribution after `n` throws from `fromPos`.
  virtual PositionVector afterNRolls(int fromPos, int n) const = 0;
};

}  // namespace monopoly::probability
```

- [ ] **Step 2: Implement `src/probability/probability_engine.h`**

```cpp
#pragma once
#include <vector>
#include "probability/distribution_provider.h"
#include "probability/transition_matrix.h"
#include "domain/board.h"
#include "domain/deck_factory.h"

namespace monopoly::probability {

// Analytic Markov implementation of IDistributionProvider.
class ProbabilityEngine : public IDistributionProvider {
 public:
  ProbabilityEngine(const domain::Board& board, const domain::Decks& decks);

  PositionVector stationaryByPosition() const override;
  double jailProbability() const override;
  PositionVector singleRoll(int fromPos) const override;
  PositionVector afterNRolls(int fromPos, int n) const override;

 private:
  std::vector<double> stepFrom(const std::vector<double>& state) const;
  static PositionVector marginalize(const std::vector<double>& state,
                                    double* jailOut);
  TransitionMatrix tm_;
};

}  // namespace monopoly::probability
```

- [ ] **Step 3: Implement `src/probability/probability_engine.cpp`**

```cpp
#include "probability/probability_engine.h"

#include "math/matrix.h"
#include "probability/micro_state.h"

namespace monopoly::probability {

ProbabilityEngine::ProbabilityEngine(const domain::Board& board,
                                     const domain::Decks& decks)
    : tm_(board, decks) {}

std::vector<double> ProbabilityEngine::stepFrom(
    const std::vector<double>& state) const {
  return tm_.matrix().vecMul(state);
}

PositionVector ProbabilityEngine::marginalize(const std::vector<double>& state,
                                              double* jailOut) {
  PositionVector pos{};
  pos.fill(0.0);
  double jail = 0.0;
  for (int i = 0; i < kNumStates; ++i) {
    if (isJailState(i)) jail += state[i];
    else pos[positionOf(i)] += state[i];
  }
  if (jailOut) *jailOut = jail;
  return pos;
}

PositionVector ProbabilityEngine::stationaryByPosition() const {
  auto pi = math::stationaryDistribution(tm_.matrix());
  double jail = 0.0;
  return marginalize(pi, &jail);
}

double ProbabilityEngine::jailProbability() const {
  auto pi = math::stationaryDistribution(tm_.matrix());
  double jail = 0.0;
  marginalize(pi, &jail);
  return jail;
}

PositionVector ProbabilityEngine::singleRoll(int fromPos) const {
  std::vector<double> state(kNumStates, 0.0);
  state[onBoardIndex(fromPos % kNumPositions, 0)] = 1.0;
  auto next = stepFrom(state);
  double jail = 0.0;
  return marginalize(next, &jail);
}

PositionVector ProbabilityEngine::afterNRolls(int fromPos, int n) const {
  std::vector<double> state(kNumStates, 0.0);
  state[onBoardIndex(fromPos % kNumPositions, 0)] = 1.0;
  for (int i = 0; i < n; ++i) state = stepFrom(state);
  double jail = 0.0;
  return marginalize(state, &jail);
}

}  // namespace monopoly::probability
```

- [ ] **Step 4: Tests** (`tests/unit/test_probability_engine.cpp`) — validation oracle.

```cpp
#include <gtest/gtest.h>
#include <cmath>
#include "probability/probability_engine.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"

using namespace monopoly::probability;

namespace {
double sum(const PositionVector& v) { double s=0; for(double x:v) s+=x; return s; }
}

TEST(ProbabilityEngine, StationaryIsDistributionAndJailDominates) {
  auto b = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto d = monopoly::domain::loadDecksFromFile(DECKS_JSON_PATH);
  ProbabilityEngine eng(b, d);
  auto pos = eng.stationaryByPosition();
  double jail = eng.jailProbability();
  EXPECT_NEAR(sum(pos) + jail, 1.0, 1e-6);
  // Jail (in-jail) is the single most-visited state in Monopoly.
  for (double x : pos) EXPECT_GE(jail, x - 1e-9);
}

TEST(ProbabilityEngine, SingleRollPeaksSevenAhead) {
  auto b = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto d = monopoly::domain::loadDecksFromFile(DECKS_JSON_PATH);
  ProbabilityEngine eng(b, d);
  // From GO(0) with no special squares in 2..12, 7-ahead (pos 7 is Chance) —
  // use pos 1 to avoid the Chance square: from 1, sum 7 -> pos 8.
  auto roll = eng.singleRoll(1);
  double s = 0; for (double x : roll) s += x;
  EXPECT_NEAR(s, 1.0, 1e-9);
  EXPECT_GT(roll[8], roll[3]);   // 7-ahead (8) more likely than 2-ahead (3)
  EXPECT_GT(roll[8], roll[13 % 40]);
}

TEST(ProbabilityEngine, TransientConvergesTowardStationary) {
  auto b = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto d = monopoly::domain::loadDecksFromFile(DECKS_JSON_PATH);
  ProbabilityEngine eng(b, d);
  auto stat = eng.stationaryByPosition();
  auto far = eng.afterNRolls(0, 200);
  double tv = 0.0;
  for (int i = 0; i < kNumPositions; ++i) tv += std::fabs(far[i] - stat[i]);
  EXPECT_LT(tv, 0.05);  // close to stationary after many rolls
}
```

- [ ] **Step 5:** Build + `ctest -R ProbabilityEngine` → PASS. **Commit** `feat(probability): engine facade + distribution provider`.

---

### Task 5: Runnable demo in `main`

**Files:** Modify `src/app/main.cpp`; modify `CMakeLists.txt` (pass data paths to the `monopoly` target).

- [ ] **Step 1: Pass data paths to the exe** in `CMakeLists.txt`:

```cmake
target_compile_definitions(monopoly PRIVATE
  BOARD_JSON_PATH="${CMAKE_SOURCE_DIR}/data/board.london.json"
  DECKS_JSON_PATH="${CMAKE_SOURCE_DIR}/data/decks.london.json")
```

- [ ] **Step 2: Implement the demo `src/app/main.cpp`**

```cpp
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "probability/probability_engine.h"

using namespace monopoly;

int main() {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::ProbabilityEngine eng(board, decks);

  auto stat = eng.stationaryByPosition();
  double jail = eng.jailProbability();

  std::cout << "Monopoly Markov Advisor — London edition\n";
  std::cout << "Long-run landing probability (top 10 squares):\n";

  std::vector<int> order(probability::kNumPositions);
  for (int i = 0; i < probability::kNumPositions; ++i) order[i] = i;
  std::sort(order.begin(), order.end(),
            [&](int a, int b) { return stat[a] > stat[b]; });

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "  JAIL (in jail)            " << jail * 100 << "%\n";
  for (int k = 0; k < 10; ++k) {
    int p = order[k];
    std::cout << "  " << std::setw(2) << p << " " << std::setw(24) << std::left
              << board.at(p).name << std::right << stat[p] * 100 << "%\n";
  }

  std::cout << "\nNext-roll landing distribution from TRAFALGAR SQUARE (24), top 5:\n";
  auto roll = eng.singleRoll(24);
  std::vector<int> ro(probability::kNumPositions);
  for (int i = 0; i < probability::kNumPositions; ++i) ro[i] = i;
  std::sort(ro.begin(), ro.end(), [&](int a, int b) { return roll[a] > roll[b]; });
  for (int k = 0; k < 5; ++k) {
    int p = ro[k];
    std::cout << "  " << std::setw(24) << std::left << board.at(p).name
              << std::right << roll[p] * 100 << "%\n";
  }
  return 0;
}
```

- [ ] **Step 3:** Build, run `./build/monopoly`, eyeball output (jail highest; sensible next-roll peak). **Commit** `feat(app): probability demo over the London board`.

---

## Milestone 3 acceptance
- `ctest` fully green; transition rows stochastic; stationary sums to 1 with jail the
  most-visited state; single-roll peaks ~7 ahead; transient → stationary.
- `./build/monopoly` prints long-run and next-roll distributions over the London board.
- All probability files < 400 LoC.
