#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "pricing/insurance_pricer.h"
#include "probability/landing.h"

using namespace monopoly;
using domain::GameState;

TEST(Pricing, NoOpponentPropertyMeansZeroPremium) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  GameState gs(board);
  int mover = gs.addPlayer("P1", 0);
  gs.player(mover).position = 0;
  pricing::PricingConfig cfg;
  auto q = pricing::priceNextRoll(gs, mover, resolver, cfg);
  EXPECT_NEAR(q.expectedRent, 0.0, 1e-6);
  EXPECT_NEAR(q.fairPremium, 0.0, 1e-6);
}

TEST(Pricing, PremiumRisesWithDevelopedOpponentProperty) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  GameState gs(board);
  int mover = gs.addPlayer("P1", 0);
  int opp = gs.addPlayer("P2", 0);
  gs.player(mover).position = 18;          // 6 ahead is 24 (Trafalgar)
  gs.setOwner(24, opp);
  gs.setOwner(21, opp); gs.setOwner(23, opp);   // complete RED monopoly
  auto qNoHouse = pricing::priceNextRoll(gs, mover, resolver, pricing::PricingConfig{});
  gs.setHouses(24, 5);                      // hotel
  auto qHotel = pricing::priceNextRoll(gs, mover, resolver, pricing::PricingConfig{});
  EXPECT_GT(qHotel.expectedRent, qNoHouse.expectedRent);
}

TEST(Pricing, MuggingBenefitReducesPremium) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  GameState gs(board);
  int mover = gs.addPlayer("P1", 0);
  int opp = gs.addPlayer("P2", 0);
  gs.player(mover).position = 18;
  gs.player(opp).position = 24;             // opponent standing 6 ahead
  pricing::PricingConfig cfg; cfg.muggingEnabled = true;
  auto q = pricing::priceNextRoll(gs, mover, resolver, cfg);
  EXPECT_GT(q.muggingExposure, 0.0);        // mover expects to gain by mugging
}
