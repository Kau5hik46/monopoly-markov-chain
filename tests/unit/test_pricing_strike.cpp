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
GameState hotelOnTrafalgar(const domain::Board& board) {
  GameState gs(board);
  gs.addPlayer("P1", 0);
  gs.addPlayer("P2", 0);
  gs.player(0).position = 18;
  gs.setOwner(24, 1); gs.setOwner(21, 1); gs.setOwner(23, 1);  // RED monopoly
  gs.setHouses(24, 5);  // hotel
  return gs;
}
}  // namespace

TEST(PricingStrike, FairPremiumAtZeroEqualsExpectedLoss) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = hotelOnTrafalgar(board);

  auto loss = pricing::buildLossDistribution(gs, 0, resolver);
  auto q = pricing::priceNextRoll(gs, 0, resolver, pricing::PricingConfig{});
  EXPECT_NEAR(pricing::fairPremiumAtStrike(loss, 0.0), q.expectedRent, 1e-6);
}

TEST(PricingStrike, PremiumIsNonIncreasingAndZeroAtMaxLoss) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = hotelOnTrafalgar(board);

  auto loss = pricing::buildLossDistribution(gs, 0, resolver);
  const double maxL = pricing::maxLossOf(loss);
  EXPECT_GT(maxL, 0.0);
  EXPECT_NEAR(pricing::fairPremiumAtStrike(loss, maxL), 0.0, 1e-6);
  EXPECT_LE(pricing::fairPremiumAtStrike(loss, maxL / 2.0),
            pricing::fairPremiumAtStrike(loss, 0.0) + 1e-9);
}

TEST(PricingStrike, EmptyLossYieldsZero) {
  std::vector<pricing::LossOutcome> empty;
  EXPECT_EQ(pricing::fairPremiumAtStrike(empty, 0.0), 0.0);
  EXPECT_EQ(pricing::fairValueAtStrike(empty, 1000.0, /*isPut=*/true), 1000.0);  // P(L=0)=1
  EXPECT_EQ(pricing::maxLossOf(empty), 0.0);
}

TEST(PricingStrike, PutAddsNoLiabilityMassAndParityHolds) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = hotelOnTrafalgar(board);
  auto loss = pricing::buildLossDistribution(gs, 0, resolver);

  const double K = pricing::maxLossOf(loss);  // high strike
  const double call = pricing::fairValueAtStrike(loss, K, /*isPut=*/false);
  const double put = pricing::fairValueAtStrike(loss, K, /*isPut=*/true);
  const double EU = pricing::fairValueAtStrike(loss, 0.0, /*isPut=*/false);  // E[U]
  // Put-call parity on the discrete distribution: call - put == E[U] - K.
  EXPECT_NEAR(call - put, EU - K, 1e-3);
  EXPECT_GE(put, 0.0);
}
