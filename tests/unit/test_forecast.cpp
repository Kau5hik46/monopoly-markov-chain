#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "probability/landing.h"
#include "risk/forecast.h"

using namespace monopoly;
using domain::GameState;

namespace {
GameState withHotelOpponent(const domain::Board& board, long moverCash) {
  GameState gs(board);
  int mover = gs.addPlayer("P1", moverCash);
  int opp = gs.addPlayer("P2", 0);
  gs.player(mover).position = 0;
  gs.setOwner(24, opp); gs.setOwner(21, opp); gs.setOwner(23, opp);  // RED set
  gs.setHouses(24, 5);                                               // hotel
  return gs;
}
}  // namespace

TEST(Forecast, CumulativeRentGrowsWithHorizon) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = withHotelOpponent(board, 1000000000);  // huge cash -> no ruin

  auto f5 = risk::forecast(gs, 0, 5, resolver, 20000, 1u);
  auto f20 = risk::forecast(gs, 0, 20, resolver, 20000, 1u);
  EXPECT_GT(f20.expectedCumulativeRent, f5.expectedCumulativeRent);
  EXPECT_NEAR(f5.ruinProbability, 0.0, 1e-9);   // can't go bankrupt with huge cash
}

TEST(Forecast, RuinProbabilityRisesWhenCashIsThin) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);

  auto rich = risk::forecast(withHotelOpponent(board, 1000000000), 0, 30, resolver, 20000, 2u);
  auto poor = risk::forecast(withHotelOpponent(board, 5000000), 0, 30, resolver, 20000, 2u);
  EXPECT_GE(poor.ruinProbability, rich.ruinProbability);
  EXPECT_GT(poor.ruinProbability, 0.0);
  EXPECT_LE(poor.ruinProbability, 1.0);
}
