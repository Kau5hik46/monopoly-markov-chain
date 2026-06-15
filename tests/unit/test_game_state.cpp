#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/game_state.h"

using namespace monopoly::domain;

TEST(GameState, PlayersAndOwnership) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p0 = gs.addPlayer("P1", 15000000);
  int p1 = gs.addPlayer("P2", 15000000);
  EXPECT_EQ(gs.numPlayers(), 2);
  EXPECT_EQ(gs.ownerOf(39), kUnowned);
  gs.setOwner(39, p0);
  EXPECT_EQ(gs.ownerOf(39), p0);
  EXPECT_EQ(gs.player(p1).cash, 15000000);
}

TEST(GameState, MonopolyAndGroupCounts) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p0 = gs.addPlayer("P1", 0);
  // Brown group = {1,3}.
  EXPECT_FALSE(gs.ownsWholeGroup(p0, ColorGroup::Brown));
  gs.setOwner(1, p0);
  gs.setOwner(3, p0);
  EXPECT_TRUE(gs.ownsWholeGroup(p0, ColorGroup::Brown));
  // Stations = {5,15,25,35}.
  gs.setOwner(5, p0); gs.setOwner(25, p0);
  EXPECT_EQ(gs.countOwnedInGroup(p0, ColorGroup::Station), 2);
}

TEST(GameState, Occupancy) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p0 = gs.addPlayer("P1", 0);
  int p1 = gs.addPlayer("P2", 0);
  gs.player(p1).position = 24;
  EXPECT_EQ(gs.occupantAt(24), p1);
  EXPECT_EQ(gs.occupantAt(24, /*exclude=*/p1), kUnowned);
  EXPECT_EQ(gs.occupantAt(10), kUnowned);
  (void)p0;
}

TEST(GameStateContracts, StoresAndIdsContracts) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  gs.addPlayer("P1", 1000);
  gs.addPlayer("P2", 1000);

  OptionContract c;
  c.id = gs.nextContractId();
  c.writer = 1; c.holder = 0; c.insured = 0; c.strike = 0; c.premium = 100;
  gs.addContract(c);

  ASSERT_EQ(gs.contracts().size(), 1u);
  EXPECT_EQ(gs.contracts()[0].id, 1);
  EXPECT_EQ(gs.nextContractId(), 2);  // monotonic

  LedgerEntry le;
  le.id = 1; le.writer = 1; le.holder = 0; le.insured = 0;
  le.strike = 0; le.premium = 100; le.escrow = 0; le.payout = 50;
  gs.ledger().push_back(le);
  EXPECT_EQ(gs.ledger().size(), 1u);
}
