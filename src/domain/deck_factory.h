#pragma once
#include <string>
#include "domain/card.h"

namespace monopoly::domain {

struct Decks {
  CardDeck chance;
  CardDeck communityChest;
};

Decks loadDecksFromFile(const std::string& path);

}  // namespace monopoly::domain
