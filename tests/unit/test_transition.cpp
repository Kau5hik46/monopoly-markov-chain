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
