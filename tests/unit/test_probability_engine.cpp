#include <gtest/gtest.h>
#include <cmath>
#include "probability/probability_engine.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"

using namespace monopoly::probability;

namespace {
double sum(const PositionVector& v) { double s = 0; for (double x : v) s += x; return s; }
}  // namespace

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
  // From pos 1, sum 7 -> pos 8. Note: a roll of 6 lands on Chance(7), and the
  // go-to-jail card diverts a sliver of mass to jail, so the position vector sums
  // to slightly under 1 (the remainder is jail mass, reported separately).
  auto roll = eng.singleRoll(1);
  double s = 0; for (double x : roll) s += x;
  EXPECT_LE(s, 1.0 + 1e-9);
  EXPECT_GT(s, 0.98);            // only a tiny fraction diverts to jail
  EXPECT_GT(roll[8], roll[3]);   // 7-ahead (8) more likely than 2-ahead (3)
  EXPECT_GT(roll[8], roll[11]);  // and than 10-ahead (11)
  // 8 is the global peak of a single roll from square 1.
  for (int p = 0; p < kNumPositions; ++p)
    if (p != 8) EXPECT_GE(roll[8], roll[p] - 1e-12);
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
