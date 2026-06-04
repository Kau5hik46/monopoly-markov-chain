#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/game_state.h"
#include "risk/rent_table.h"

using namespace monopoly::domain;
using monopoly::risk::rentOwed;

TEST(Rent, StreetSiteMonopolyAndHouses) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int owner = gs.addPlayer("P1", 0);
  int mover = gs.addPlayer("P2", 0);
  const int pos = 1;  // Portobello Road Market = Old Kent Road analog
  gs.setOwner(pos, owner);
  EXPECT_EQ(rentOwed(gs, pos, mover, 7), 20000);          // site (£2 x10000)
  gs.setOwner(3, owner);                                   // complete BROWN
  EXPECT_EQ(rentOwed(gs, pos, mover, 7), 40000);           // undeveloped monopoly (2x)
  gs.setHouses(pos, 1);
  EXPECT_EQ(rentOwed(gs, pos, mover, 7), 100000);          // 1 house (£10 x10000)
}

TEST(Rent, MortgageAndOwnerExemptions) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int owner = gs.addPlayer("P1", 0);
  int mover = gs.addPlayer("P2", 0);
  gs.setOwner(1, owner);
  gs.setMortgaged(1, true);
  EXPECT_EQ(rentOwed(gs, 1, mover, 7), 0);                 // mortgaged
  gs.setMortgaged(1, false);
  EXPECT_EQ(rentOwed(gs, 1, owner, 7), 0);                 // owner pays no rent
}

TEST(Rent, StationByCountAndUtilityByDice) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int owner = gs.addPlayer("P1", 0);
  int mover = gs.addPlayer("P2", 0);
  gs.setOwner(5, owner);                                   // 1 station
  EXPECT_EQ(rentOwed(gs, 5, mover, 7), 250000);
  gs.setOwner(15, owner);                                  // 2 stations
  EXPECT_EQ(rentOwed(gs, 5, mover, 7), 500000);
  gs.setOwner(12, owner);                                  // 1 utility
  EXPECT_EQ(rentOwed(gs, 12, mover, 9), 40000 * 9);
}
