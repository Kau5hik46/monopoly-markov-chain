#include <gtest/gtest.h>
#include "domain/deck_factory.h"

#ifndef DECKS_JSON_PATH
#define DECKS_JSON_PATH "data/decks.london.json"
#endif

using namespace monopoly::domain;

TEST(Deck, ChanceHasSixteenCards) {
  auto decks = loadDecksFromFile(DECKS_JSON_PATH);
  EXPECT_EQ(decks.chance.size(), 16);
}

TEST(Deck, CommunityChestHasSixteenCards) {
  auto decks = loadDecksFromFile(DECKS_JSON_PATH);
  EXPECT_EQ(decks.communityChest.size(), 16);
}

TEST(Deck, ChanceMovementWeightsAreCanonical) {
  auto decks = loadDecksFromFile(DECKS_JSON_PATH);
  int movers = 0;
  for (const auto& e : decks.chance.effects)
    if (e.move != CardMove::None) movers += e.weight;
  EXPECT_EQ(movers, 10);  // 10 movement cards, 6 money cards
}
