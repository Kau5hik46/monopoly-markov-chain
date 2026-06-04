# Milestone 1: Foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up the CMake/C++17 project with GoogleTest, a dense `Matrix` with a stationary-distribution solver, and a `DiceModel` (2d6 joint table + mugging contest probabilities) — all unit-tested and building green.

**Architecture:** Single static library `monopoly_core` with two namespaces this milestone: `monopoly::math` (linear algebra) and `monopoly::probability` (dice). A `monopoly` REPL executable is a stub banner for now; `monopoly_tests` runs GoogleTest via CTest.

**Tech Stack:** C++17, CMake (>=3.16) with FetchContent GoogleTest, clang/libc++, Make.

---

## File structure (this milestone)

- Create: `CMakeLists.txt` — root build, library + exe + tests, FetchContent gtest.
- Create: `src/math/matrix.h` / `src/math/matrix.cpp` — `Matrix`, `vecMul`, `operator*`, `stationaryDistribution`.
- Create: `src/probability/dice.h` / `src/probability/dice.cpp` — 2d6 PMF, doubles, mugging contest probabilities.
- Create: `src/app/main.cpp` — stub REPL banner so the exe target links.
- Create: `tests/unit/test_matrix.cpp`, `tests/unit/test_dice.cpp`.
- Create: `.gitignore` additions for `build/`.

---

### Task 1: Project skeleton + GoogleTest + green empty test

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/app/main.cpp`
- Create: `tests/unit/test_smoke.cpp`
- Modify: `.gitignore`

- [ ] **Step 1: Write the root CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(monopoly_advisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Core library (sources added as milestones progress) ---
add_library(monopoly_core
  src/math/matrix.cpp
  src/probability/dice.cpp
)
target_include_directories(monopoly_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_features(monopoly_core PUBLIC cxx_std_17)

# --- REPL executable ---
add_executable(monopoly src/app/main.cpp)
target_link_libraries(monopoly PRIVATE monopoly_core)

# --- Tests via FetchContent GoogleTest ---
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(monopoly_tests
  tests/unit/test_smoke.cpp
  tests/unit/test_matrix.cpp
  tests/unit/test_dice.cpp
)
target_link_libraries(monopoly_tests PRIVATE monopoly_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(monopoly_tests)
```

- [ ] **Step 2: Create the stub REPL and smoke test, and stub source files referenced by CMake**

`src/app/main.cpp`:
```cpp
#include <iostream>

int main() {
  std::cout << "Monopoly Markov Advisor (foundation build)\n";
  return 0;
}
```

`tests/unit/test_smoke.cpp`:
```cpp
#include <gtest/gtest.h>

TEST(Smoke, BuildAndRun) {
  EXPECT_EQ(1 + 1, 2);
}
```

Create empty-but-valid stubs so CMake's source list compiles before later tasks fill them in. `src/math/matrix.cpp`:
```cpp
#include "math/matrix.h"
```
`src/probability/dice.cpp`:
```cpp
#include "probability/dice.h"
```
And minimal headers so the includes resolve. `src/math/matrix.h`:
```cpp
#pragma once
```
`src/probability/dice.h`:
```cpp
#pragma once
```
Also create placeholder test files so the test target links — `tests/unit/test_matrix.cpp` and `tests/unit/test_dice.cpp` each:
```cpp
#include <gtest/gtest.h>
```

- [ ] **Step 3: Add build/ to .gitignore**

Append to `.gitignore`:
```
/build/
```

- [ ] **Step 4: Configure, build, and run tests — verify green**

Run:
```bash
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
```
Expected: configures (downloads googletest), builds `monopoly_core`, `monopoly`, `monopoly_tests`; CTest reports the `Smoke.BuildAndRun` test PASS.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/app/main.cpp src/math/matrix.h src/math/matrix.cpp \
        src/probability/dice.h src/probability/dice.cpp \
        tests/unit/test_smoke.cpp tests/unit/test_matrix.cpp tests/unit/test_dice.cpp .gitignore
git commit -m "build: scaffold CMake C++17 project with GoogleTest"
```

---

### Task 2: Matrix multiply and row-vector product

**Files:**
- Modify: `src/math/matrix.h`
- Modify: `src/math/matrix.cpp`
- Modify: `tests/unit/test_matrix.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `tests/unit/test_matrix.cpp` with:
```cpp
#include <gtest/gtest.h>
#include "math/matrix.h"

using monopoly::math::Matrix;

TEST(Matrix, ElementAccessAndDims) {
  Matrix m(2, 3, 0.0);
  EXPECT_EQ(m.rows(), 2u);
  EXPECT_EQ(m.cols(), 3u);
  m(1, 2) = 4.5;
  EXPECT_DOUBLE_EQ(m(1, 2), 4.5);
  EXPECT_DOUBLE_EQ(m(0, 0), 0.0);
}

TEST(Matrix, Multiply) {
  Matrix a(2, 2, 0.0);
  a(0, 0) = 1; a(0, 1) = 2; a(1, 0) = 3; a(1, 1) = 4;
  Matrix b(2, 2, 0.0);
  b(0, 0) = 5; b(0, 1) = 6; b(1, 0) = 7; b(1, 1) = 8;
  Matrix c = a * b;  // [[19,22],[43,50]]
  EXPECT_DOUBLE_EQ(c(0, 0), 19);
  EXPECT_DOUBLE_EQ(c(0, 1), 22);
  EXPECT_DOUBLE_EQ(c(1, 0), 43);
  EXPECT_DOUBLE_EQ(c(1, 1), 50);
}

TEST(Matrix, RowVectorTimesMatrix) {
  // result[j] = sum_i v[i] * M(i,j)
  Matrix m(2, 2, 0.0);
  m(0, 0) = 1; m(0, 1) = 2; m(1, 0) = 3; m(1, 1) = 4;
  auto r = m.vecMul({1.0, 1.0});  // {1*1+1*3, 1*2+1*4} = {4,6}
  ASSERT_EQ(r.size(), 2u);
  EXPECT_DOUBLE_EQ(r[0], 4);
  EXPECT_DOUBLE_EQ(r[1], 6);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build -j && ctest --test-dir build -R Matrix --output-on-failure`
Expected: compile error / FAIL — `Matrix` not yet defined.

- [ ] **Step 3: Implement the Matrix header**

Replace `src/math/matrix.h` with:
```cpp
#pragma once
#include <cstddef>
#include <vector>

namespace monopoly::math {

// Dense, row-major matrix of doubles.
class Matrix {
 public:
  Matrix() = default;
  Matrix(std::size_t rows, std::size_t cols, double init = 0.0)
      : rows_(rows), cols_(cols), data_(rows * cols, init) {}

  double& operator()(std::size_t r, std::size_t c) { return data_[r * cols_ + c]; }
  double operator()(std::size_t r, std::size_t c) const { return data_[r * cols_ + c]; }

  std::size_t rows() const noexcept { return rows_; }
  std::size_t cols() const noexcept { return cols_; }

  // Standard matrix product (this * rhs). Requires cols() == rhs.rows().
  Matrix operator*(const Matrix& rhs) const;

  // Row-vector times matrix: result[j] = sum_i v[i] * (*this)(i, j).
  // Requires v.size() == rows().
  [[nodiscard]] std::vector<double> vecMul(const std::vector<double>& v) const;

 private:
  std::size_t rows_ = 0;
  std::size_t cols_ = 0;
  std::vector<double> data_;  // row-major, size rows_*cols_
};

}  // namespace monopoly::math
```

- [ ] **Step 4: Implement the Matrix source**

Replace `src/math/matrix.cpp` with:
```cpp
#include "math/matrix.h"

namespace monopoly::math {

Matrix Matrix::operator*(const Matrix& rhs) const {
  Matrix out(rows_, rhs.cols_, 0.0);
  for (std::size_t i = 0; i < rows_; ++i) {
    for (std::size_t k = 0; k < cols_; ++k) {
      const double aik = (*this)(i, k);
      if (aik == 0.0) continue;
      for (std::size_t j = 0; j < rhs.cols_; ++j) {
        out(i, j) += aik * rhs(k, j);
      }
    }
  }
  return out;
}

std::vector<double> Matrix::vecMul(const std::vector<double>& v) const {
  std::vector<double> out(cols_, 0.0);
  for (std::size_t i = 0; i < rows_; ++i) {
    const double vi = v[i];
    if (vi == 0.0) continue;
    for (std::size_t j = 0; j < cols_; ++j) {
      out[j] += vi * (*this)(i, j);
    }
  }
  return out;
}

}  // namespace monopoly::math
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build -j && ctest --test-dir build -R Matrix --output-on-failure`
Expected: all 3 Matrix tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/math/matrix.h src/math/matrix.cpp tests/unit/test_matrix.cpp
git commit -m "feat(math): dense Matrix with multiply and row-vector product"
```

---

### Task 3: Stationary distribution via power iteration

**Files:**
- Modify: `src/math/matrix.h`
- Modify: `src/math/matrix.cpp`
- Modify: `tests/unit/test_matrix.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/unit/test_matrix.cpp`:
```cpp
#include <cmath>
#include "math/matrix.h"

using monopoly::math::stationaryDistribution;

TEST(Stationary, TwoStateChain) {
  // P = [[0.9,0.1],[0.5,0.5]]  =>  pi = [5/6, 1/6]
  Matrix p(2, 2, 0.0);
  p(0, 0) = 0.9; p(0, 1) = 0.1;
  p(1, 0) = 0.5; p(1, 1) = 0.5;
  auto pi = stationaryDistribution(p);
  ASSERT_EQ(pi.size(), 2u);
  EXPECT_NEAR(pi[0], 5.0 / 6.0, 1e-9);
  EXPECT_NEAR(pi[1], 1.0 / 6.0, 1e-9);
  EXPECT_NEAR(pi[0] + pi[1], 1.0, 1e-12);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j 2>&1 | head`
Expected: compile error — `stationaryDistribution` not declared.

- [ ] **Step 3: Declare the solver in the header**

Add to `src/math/matrix.h` before the closing namespace brace:
```cpp
// Stationary distribution pi solving pi*P = pi for a row-stochastic P
// (each row sums to 1). Computed by power iteration from the uniform vector.
[[nodiscard]] std::vector<double> stationaryDistribution(
    const Matrix& P, double tol = 1e-12, std::size_t maxIter = 1000000);
```

- [ ] **Step 4: Implement the solver**

Add to `src/math/matrix.cpp` before the closing namespace brace (add `#include <cmath>` at top):
```cpp
std::vector<double> stationaryDistribution(const Matrix& P, double tol,
                                           std::size_t maxIter) {
  const std::size_t n = P.rows();
  std::vector<double> v(n, 1.0 / static_cast<double>(n));
  for (std::size_t iter = 0; iter < maxIter; ++iter) {
    std::vector<double> next = P.vecMul(v);
    double sum = 0.0;
    for (double x : next) sum += x;
    if (sum > 0.0) {
      for (double& x : next) x /= sum;  // guard against drift
    }
    double diff = 0.0;
    for (std::size_t i = 0; i < n; ++i) diff += std::fabs(next[i] - v[i]);
    v.swap(next);
    if (diff < tol) break;
  }
  return v;
}
```
Add `#include <cmath>` to the top of `matrix.cpp`.

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build -j && ctest --test-dir build -R Stationary --output-on-failure`
Expected: `Stationary.TwoStateChain` PASS.

- [ ] **Step 6: Commit**

```bash
git add src/math/matrix.h src/math/matrix.cpp tests/unit/test_matrix.cpp
git commit -m "feat(math): stationary distribution via power iteration"
```

---

### Task 4: DiceModel — 2d6 PMF and doubles

**Files:**
- Modify: `src/probability/dice.h`
- Modify: `src/probability/dice.cpp`
- Modify: `tests/unit/test_dice.cpp`

- [ ] **Step 1: Write the failing tests**

Replace `tests/unit/test_dice.cpp` with:
```cpp
#include <gtest/gtest.h>
#include "probability/dice.h"

namespace dice = monopoly::probability;

TEST(Dice, PmfSumsToOne) {
  auto pmf = dice::twoD6Pmf();
  double sum = 0.0;
  for (double p : pmf) sum += p;
  EXPECT_NEAR(sum, 1.0, 1e-12);
}

TEST(Dice, PmfKnownValues) {
  auto pmf = dice::twoD6Pmf();
  EXPECT_NEAR(pmf[2], 1.0 / 36.0, 1e-12);
  EXPECT_NEAR(pmf[7], 6.0 / 36.0, 1e-12);
  EXPECT_NEAR(pmf[12], 1.0 / 36.0, 1e-12);
  EXPECT_DOUBLE_EQ(pmf[0], 0.0);
  EXPECT_DOUBLE_EQ(pmf[1], 0.0);
}

TEST(Dice, DoubleProbability) {
  EXPECT_NEAR(dice::pDouble(), 6.0 / 36.0, 1e-12);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build -j 2>&1 | head`
Expected: compile error — `twoD6Pmf` not declared.

- [ ] **Step 3: Implement the dice header**

Replace `src/probability/dice.h` with:
```cpp
#pragma once
#include <array>

namespace monopoly::probability {

// PMF of the sum of two fair 6-sided dice, indexed by sum (0..12).
// pmf[0] = pmf[1] = 0. Sums to 1.
constexpr std::array<double, 13> twoD6Pmf() {
  std::array<double, 13> pmf{};
  for (int a = 1; a <= 6; ++a)
    for (int b = 1; b <= 6; ++b)
      pmf[a + b] += 1.0 / 36.0;
  return pmf;
}

// Probability a single 2d6 roll shows a double (6/36).
constexpr double pDouble() { return 6.0 / 36.0; }

// Mugging contest: mugger and muggee each roll a fresh 2d6; muggee escapes on >=.
double pMuggerWins();     // P(mugger sum  >  muggee sum)
double pTie();            // P(equal sums)
double pMuggeeEscapes();  // P(muggee sum >= mugger sum) = pMuggerWins() + pTie()

}  // namespace monopoly::probability
```

- [ ] **Step 4: Implement the dice source (doubles part only here)**

Replace `src/probability/dice.cpp` with the contest functions (used by the next task; declaring here keeps one source file):
```cpp
#include "probability/dice.h"

namespace monopoly::probability {

double pTie() {
  auto pmf = twoD6Pmf();
  double p = 0.0;
  for (int s = 2; s <= 12; ++s) p += pmf[s] * pmf[s];
  return p;
}

double pMuggerWins() {
  auto pmf = twoD6Pmf();
  double p = 0.0;
  for (int hi = 2; hi <= 12; ++hi)
    for (int lo = 2; lo < hi; ++lo)
      p += pmf[hi] * pmf[lo];
  return p;
}

double pMuggeeEscapes() { return pMuggerWins() + pTie(); }

}  // namespace monopoly::probability
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build -j && ctest --test-dir build -R Dice --output-on-failure`
Expected: `Dice.PmfSumsToOne`, `Dice.PmfKnownValues`, `Dice.DoubleProbability` PASS.

- [ ] **Step 6: Commit**

```bash
git add src/probability/dice.h src/probability/dice.cpp tests/unit/test_dice.cpp
git commit -m "feat(probability): 2d6 PMF and doubles probability"
```

---

### Task 5: Mugging contest probabilities

**Files:**
- Modify: `tests/unit/test_dice.cpp`

(Implementation already added in Task 4; this task pins the contract with tests.)

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/test_dice.cpp`:
```cpp
TEST(Dice, MuggingContestProbabilities) {
  // Two independent 2d6 sums. Ties go to the muggee (needs >=).
  EXPECT_NEAR(dice::pMuggerWins(), 0.4437, 1e-3);
  EXPECT_NEAR(dice::pTie(), 0.1127, 1e-3);
  EXPECT_NEAR(dice::pMuggeeEscapes(), 0.5563, 1e-3);
}

TEST(Dice, MuggingProbabilitiesAreExhaustive) {
  // mugger-wins + muggee-escapes must cover all outcomes exactly once.
  EXPECT_NEAR(dice::pMuggerWins() + dice::pMuggeeEscapes(), 1.0, 1e-12);
  // Symmetry: P(mugger>muggee) == P(muggee>mugger) == (1 - tie)/2.
  EXPECT_NEAR(dice::pMuggerWins(), (1.0 - dice::pTie()) / 2.0, 1e-12);
}
```

- [ ] **Step 2: Run tests to verify they pass**

Run: `cmake --build build -j && ctest --test-dir build -R Dice --output-on-failure`
Expected: all Dice tests PASS (implementation exists from Task 4).

- [ ] **Step 3: Commit**

```bash
git add tests/unit/test_dice.cpp
git commit -m "test(probability): pin mugging contest probabilities and invariants"
```

---

### Task 6: Remove dead scaffold replaced by the new layout

**Files:**
- Delete: `math/vector.h`, `state_space/markov_chain.h`, `state_space/state.h`, `strategy/strategy.h` (non-compiling stubs superseded by `src/`).

> Keep `accounts/`, `properties/`, `players/` for now — M2 replaces them with `src/domain/`. Removing only the files with no forward use this milestone keeps each commit focused.

- [ ] **Step 1: Remove the superseded stub files**

Run:
```bash
git rm math/vector.h state_space/markov_chain.h state_space/state.h strategy/strategy.h
rmdir math state_space strategy 2>/dev/null || true
```

- [ ] **Step 2: Rebuild and re-run tests — verify nothing references them**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: full build green; all tests PASS.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "chore: remove non-compiling state_space/strategy/math stubs superseded by src/"
```

---

## Milestone 1 acceptance
- `cmake -S . -B build && cmake --build build -j && ctest --test-dir build` is green.
- `monopoly` exe runs and prints the foundation banner.
- `monopoly_core` exposes `monopoly::math::Matrix` + `stationaryDistribution` and `monopoly::probability` dice/mugging probabilities, all unit-tested.
- Dead `state_space/`, `strategy/`, `math/vector.h` scaffold removed.
