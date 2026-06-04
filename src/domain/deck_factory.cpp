#include "domain/deck_factory.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

namespace monopoly::domain {

namespace {
CardMove moveFromString(const std::string& s) {
  if (s == "NONE") return CardMove::None;
  if (s == "ADVANCE_TO") return CardMove::AdvanceTo;
  if (s == "GO_TO_JAIL") return CardMove::GoToJail;
  if (s == "NEAREST_STATION") return CardMove::NearestStation;
  if (s == "NEAREST_UTILITY") return CardMove::NearestUtility;
  if (s == "BACK_3") return CardMove::Back3;
  throw std::runtime_error("unknown card move: " + s);
}

CardDeck parseDeck(const nlohmann::json& arr) {
  CardDeck deck;
  for (const auto& e : arr) {
    CardEffect c;
    c.move = moveFromString(e.at("move").get<std::string>());
    c.target = e.at("target").get<int>();
    c.weight = e.at("weight").get<int>();
    deck.effects.push_back(c);
  }
  return deck;
}
}  // namespace

Decks loadDecksFromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open decks file: " + path);
  nlohmann::json j;
  in >> j;
  Decks d;
  d.chance = parseDeck(j.at("chance"));
  d.communityChest = parseDeck(j.at("community_chest"));
  return d;
}

}  // namespace monopoly::domain
