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
