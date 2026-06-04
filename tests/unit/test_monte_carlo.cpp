#include <gtest/gtest.h>
#include <cmath>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "probability/monte_carlo.h"
#include "probability/probability_engine.h"

using namespace monopoly::probability;

namespace {
double totalVariation(const PositionVector& a, const PositionVector& b) {
  double tv = 0.0;
  for (int i = 0; i < kNumPositions; ++i) tv += std::fabs(a[i] - b[i]);
  return 0.5 * tv;
}
}  // namespace

// The validation oracle: with no coupling rules, Monte-Carlo must reproduce the
// analytic stationary distribution within tolerance.
TEST(MonteCarlo, StationaryMatchesAnalytic) {
  auto b = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto d = monopoly::domain::loadDecksFromFile(DECKS_JSON_PATH);
  ProbabilityEngine analytic(b, d);
  MonteCarloEngine mc(b, d, /*seed=*/42u, /*walkSteps=*/2000000);

  auto pa = analytic.stationaryByPosition();
  auto pm = mc.stationaryByPosition();
  EXPECT_LT(totalVariation(pa, pm), 0.02);

  // Jail probability also agrees.
  EXPECT_NEAR(analytic.jailProbability(), mc.jailProbability(), 0.01);
}

TEST(MonteCarlo, SingleRollPeakAgrees) {
  auto b = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto d = monopoly::domain::loadDecksFromFile(DECKS_JSON_PATH);
  ProbabilityEngine analytic(b, d);
  MonteCarloEngine mc(b, d, /*seed=*/7u, 1000, /*trials=*/300000);

  auto ra = analytic.singleRoll(1);
  auto rm = mc.singleRoll(1);
  // Both peak at 8 (seven ahead) and roughly agree there.
  EXPECT_NEAR(ra[8], rm[8], 0.01);
  EXPECT_NEAR(ra[8], 6.0 / 36.0, 1e-9);
}
