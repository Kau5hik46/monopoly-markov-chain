#include <gtest/gtest.h>
#include <algorithm>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "pricing/option_chain.h"
#include "probability/landing.h"

using namespace monopoly;
using domain::GameState;

namespace {
// P2 owns a RED-group hotel on Trafalgar (24). Returns the state; callers position
// the landers (P1, P3) where they like.
GameState ownerWithHotel(const domain::Board& board, int players) {
  GameState gs(board);
  for (int i = 0; i < players; ++i) gs.addPlayer("P" + std::to_string(i + 1), 0);
  gs.setOwner(24, 1); gs.setOwner(21, 1); gs.setOwner(23, 1);  // P2 RED monopoly
  gs.setHouses(24, 5);  // hotel
  return gs;
}
}  // namespace

TEST(IncomeChain, SingleLanderMatchesThatLandersRentToOwner) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = ownerWithHotel(board, 3);
  gs.player(0).position = 18;  // P1 within reach of 24

  auto income = pricing::buildIncomeDistribution(gs, /*owner=*/1, {0}, resolver);
  // E[income] from P1 alone == P1's own expected rent (all of which goes to owner P2,
  // since P2 only owns the RED group and that's the only opponent-owned rent P1 faces
  // here — P3 owns nothing).
  const double eIncome = pricing::fairValueAtStrike(income, 0.0, false);
  auto p1Loss = pricing::buildLossDistribution(gs, 0, resolver);
  const double eP1Rent = pricing::fairValueAtStrike(p1Loss, 0.0, false);
  EXPECT_NEAR(eIncome, eP1Rent, 1e-6);
  EXPECT_GT(eIncome, 0.0);
}

TEST(IncomeChain, TwoLandersExpectationIsAdditive) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = ownerWithHotel(board, 3);
  gs.player(0).position = 18;  // P1
  gs.player(2).position = 20;  // P3 also within reach of 24

  const double e1 = pricing::fairValueAtStrike(
      pricing::buildIncomeDistribution(gs, 1, {0}, resolver), 0.0, false);
  const double e3 = pricing::fairValueAtStrike(
      pricing::buildIncomeDistribution(gs, 1, {2}, resolver), 0.0, false);
  const double both = pricing::fairValueAtStrike(
      pricing::buildIncomeDistribution(gs, 1, {0, 2}, resolver), 0.0, false);
  EXPECT_NEAR(both, e1 + e3, 1.0);  // linearity of expectation through convolution
}

TEST(IncomeChain, ReachableLandersExcludesOwnerAndJailedPlayer) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = ownerWithHotel(board, 3);
  gs.player(0).position = 18;   // P1 in reach of the RED group
  gs.player(2).position = 20;   // P3 in reach too...
  gs.player(2).inJail = true;   // ...but jailed, so it does not roll this round

  auto landers = pricing::reachableLanders(gs, /*owner=*/1, resolver);
  EXPECT_NE(std::find(landers.begin(), landers.end(), 0), landers.end());   // P1 in
  EXPECT_EQ(std::find(landers.begin(), landers.end(), 1), landers.end());   // owner out
  EXPECT_EQ(std::find(landers.begin(), landers.end(), 2), landers.end());   // jailed out
}

TEST(IncomeChain, EmptyLandersYieldsNoIncome) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = ownerWithHotel(board, 2);
  auto income = pricing::buildIncomeDistribution(gs, 1, {}, resolver);
  EXPECT_TRUE(income.empty());
  EXPECT_EQ(pricing::fairValueAtStrike(income, 0.0, false), 0.0);
}
