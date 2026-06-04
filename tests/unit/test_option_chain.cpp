#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "pricing/insurance_pricer.h"
#include "pricing/option_chain.h"
#include "probability/landing.h"

using namespace monopoly;
using domain::GameState;

namespace {
GameState withHotelOnTrafalgar(const domain::Board& board) {
  GameState gs(board);
  int mover = gs.addPlayer("P1", 0);
  int opp = gs.addPlayer("P2", 0);
  gs.player(mover).position = 18;
  gs.setOwner(24, opp);
  gs.setOwner(21, opp);
  gs.setOwner(23, opp);   // complete RED monopoly
  gs.setHouses(24, 5);    // hotel
  return gs;
}
}  // namespace

TEST(OptionChain, PremiumAtZeroEqualsExpectedLoss) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = withHotelOnTrafalgar(board);

  auto chain = pricing::buildOptionChain(gs, 0, resolver);
  ASSERT_FALSE(chain.rows.empty());
  EXPECT_EQ(chain.rows.front().strike, 0.0);
  // premium(0) == E[L] == the pricer's expectedRent.
  auto q = pricing::priceNextRoll(gs, 0, resolver, pricing::PricingConfig{});
  EXPECT_NEAR(chain.rows.front().fairPremium, q.expectedRent, 1e-6);
  EXPECT_NEAR(chain.expectedLoss, q.expectedRent, 1e-6);
  EXPECT_DOUBLE_EQ(chain.maxLoss, 11000000.0);
}

TEST(OptionChain, PremiumAndPayoutAreMonotoneInStrike) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = withHotelOnTrafalgar(board);

  auto chain = pricing::buildOptionChain(gs, 0, resolver);
  for (std::size_t i = 1; i < chain.rows.size(); ++i) {
    EXPECT_LE(chain.rows[i].fairPremium, chain.rows[i - 1].fairPremium + 1e-9);
    EXPECT_LE(chain.rows[i].payoutProb, chain.rows[i - 1].payoutProb + 1e-9);
    EXPECT_GE(chain.rows[i].strike, chain.rows[i - 1].strike);
  }
  // Top strike (>= maxLoss) has zero premium and zero payout probability.
  EXPECT_NEAR(chain.rows.back().fairPremium, 0.0, 1e-6);
  EXPECT_NEAR(chain.rows.back().payoutProb, 0.0, 1e-9);
}

TEST(OptionChain, NoLiabilityYieldsSingleZeroRow) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  GameState gs(board);
  gs.addPlayer("P1", 0);
  gs.player(0).position = 0;  // nothing owned -> no liability
  auto chain = pricing::buildOptionChain(gs, 0, resolver);
  EXPECT_EQ(chain.maxLoss, 0.0);
  ASSERT_EQ(chain.rows.size(), 1u);
  EXPECT_EQ(chain.rows.front().fairPremium, 0.0);
}
