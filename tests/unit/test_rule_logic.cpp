#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/game_state.h"
#include "rules/rule_logic.h"

using namespace monopoly::domain;
using namespace monopoly::rules;

TEST(RuleLogic, MuggingTiesFavorMuggee) {
  EXPECT_EQ(resolveMugging(9, 5), MuggingResult::MuggerWins);
  EXPECT_EQ(resolveMugging(7, 7), MuggingResult::MuggeeEscapes);  // tie -> muggee
  EXPECT_EQ(resolveMugging(4, 10), MuggingResult::MuggeeEscapes);
}

TEST(RuleLogic, FreeParkingHousesPerTax) {
  RuleConfig cfg;  // defaults: income 2, super 1, enabled
  EXPECT_EQ(freeParkingHousesForTax(SquareType::IncomeTax, cfg), 2);
  EXPECT_EQ(freeParkingHousesForTax(SquareType::SuperTax, cfg), 1);
  EXPECT_EQ(freeParkingHousesForTax(SquareType::Street, cfg), 0);
  cfg.freeParkingPotEnabled = false;
  EXPECT_EQ(freeParkingHousesForTax(SquareType::IncomeTax, cfg), 0);
}

TEST(RuleLogic, MuggingEligibilityExcludesJailAndFreeParking) {
  EXPECT_TRUE(isMuggingEligible(SquareType::Street));
  EXPECT_FALSE(isMuggingEligible(SquareType::Jail));
  EXPECT_FALSE(isMuggingEligible(SquareType::FreeParking));
}

TEST(RuleLogic, AirportTravelRequiresBothOwned) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p = gs.addPlayer("P1", 0);
  EXPECT_FALSE(canTravelBetweenAirports(gs, p, 5, 15));  // none owned
  gs.setOwner(5, p);
  EXPECT_FALSE(canTravelBetweenAirports(gs, p, 5, 15));  // only one owned
  gs.setOwner(15, p);
  EXPECT_TRUE(canTravelBetweenAirports(gs, p, 5, 15));
  EXPECT_FALSE(canTravelBetweenAirports(gs, p, 5, 5));   // same airport
  EXPECT_FALSE(canTravelBetweenAirports(gs, p, 5, 6));   // 6 is not a station
}
