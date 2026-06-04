#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "probability/landing.h"
#include "rules/rule_config.h"
#include "risk/game_sim.h"

using namespace monopoly;
using domain::GameState;

TEST(GameSim, OutcomesPerPlayerAreValid) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  rules::RuleConfig rules;  // mugging on by default

  GameState gs(board);
  int a = gs.addPlayer("P1", 15000000);
  int b = gs.addPlayer("P2", 15000000);
  // P2 owns a developed monopoly -> P1 expects to bleed cash.
  gs.setOwner(24, b); gs.setOwner(21, b); gs.setOwner(23, b);
  gs.setHouses(24, 5);
  (void)a;

  auto fc = risk::simulateGame(gs, rules, 15, resolver, 5000, 7u);
  ASSERT_EQ(fc.players.size(), 2u);
  for (const auto& o : fc.players) {
    EXPECT_GE(o.ruinProbability, 0.0);
    EXPECT_LE(o.ruinProbability, 1.0);
    EXPECT_GE(o.expectedTimesRobbed, 0.0);
  }
  // The landlord (P2) ends richer on average than the tenant (P1).
  EXPECT_GT(fc.players[1].expectedCashDelta, fc.players[0].expectedCashDelta);
}

TEST(GameSim, MuggingCouplingProducesRobberies) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  rules::RuleConfig rules; rules.muggingEnabled = true;

  GameState gs(board);
  gs.addPlayer("P1", 15000000);
  gs.addPlayer("P2", 15000000);
  auto on = risk::simulateGame(gs, rules, 20, resolver, 4000, 3u);
  double totalRobbed = on.players[0].expectedTimesRobbed + on.players[1].expectedTimesRobbed;
  EXPECT_GT(totalRobbed, 0.0);  // collisions happen over 20 rounds

  rules.muggingEnabled = false;
  auto off = risk::simulateGame(gs, rules, 20, resolver, 4000, 3u);
  EXPECT_EQ(off.players[0].expectedTimesRobbed + off.players[1].expectedTimesRobbed, 0.0);
}
