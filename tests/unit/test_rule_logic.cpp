#include <gtest/gtest.h>
#include <cstdlib>
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

TEST(RuleLogic, FreeParkingPlacesOnMonopolyEvenly) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p = gs.addPlayer("P1", 0);
  gs.setOwner(1, p);                 // BROWN group = {1,3}
  gs.setOwner(3, p);
  gs.setFreeParkingPot(3);
  auto cr = claimFreeParkingPot(gs, p);
  EXPECT_EQ(cr.placed, 3);
  EXPECT_EQ(cr.cashed, 0);
  EXPECT_EQ(gs.housesOn(1) + gs.housesOn(3), 3);
  // even-building: the two streets differ by at most one house.
  EXPECT_LE(std::abs(gs.housesOn(1) - gs.housesOn(3)), 1);
  EXPECT_EQ(gs.freeParkingPot(), 0);
}

TEST(RuleLogic, FreeParkingCashesOutWithoutMonopoly) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p = gs.addPlayer("P1", 0);
  gs.setOwner(1, p);                 // owns only half of BROWN -> no monopoly
  gs.setFreeParkingPot(2);
  auto cr = claimFreeParkingPot(gs, p);
  EXPECT_EQ(cr.placed, 0);
  EXPECT_EQ(cr.cashed, 2);
  EXPECT_EQ(gs.housesOn(1), 0);
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
