#pragma once
#include <vector>

namespace monopoly::domain {

enum class CardMove {
  None,          // money/other card; no movement
  AdvanceTo,     // go to target position
  GoToJail,      // straight to jail (in-jail)
  NearestStation,
  NearestUtility,
  Back3
};

struct CardEffect {
  CardMove move = CardMove::None;
  int target = 0;   // used by AdvanceTo
  int weight = 1;   // number of cards in the deck with this effect
};

// A deck is a weighted list of effects; weights sum to the deck size.
struct CardDeck {
  std::vector<CardEffect> effects;
  int size() const {
    int n = 0;
    for (const auto& e : effects) n += e.weight;
    return n;
  }
};

}  // namespace monopoly::domain
