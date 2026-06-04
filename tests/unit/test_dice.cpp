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
