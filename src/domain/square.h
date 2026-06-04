#pragma once
#include <array>
#include <string>
#include <string_view>
#include "domain/color_group.h"

namespace monopoly::domain {

enum class SquareType {
  Go, Street, Station, Utility, IncomeTax, SuperTax,
  Chance, CommunityChest, Jail, GoToJail, FreeParking
};

// Flat value type for a board square. Purchase fields are 0 for non-property squares.
struct Square {
  int position = 0;
  std::string name;
  SquareType type = SquareType::Go;
  ColorGroup group = ColorGroup::None;
  long price = 0;       // listed purchase price
  long houseCost = 0;   // per-house build cost (streets only)
  long mortgage = 0;    // mortgage value
  // Street rent schedule [site, 1h, 2h, 3h, 4h, hotel]; zeros for non-streets.
  std::array<long, 6> rent{};
};

inline bool isPurchasable(SquareType t) {
  return t == SquareType::Street || t == SquareType::Station ||
         t == SquareType::Utility;
}

inline SquareType squareTypeFromString(std::string_view s) {
  if (s == "GO") return SquareType::Go;
  if (s == "STREET") return SquareType::Street;
  if (s == "STATION") return SquareType::Station;
  if (s == "UTILITY") return SquareType::Utility;
  if (s == "INCOME_TAX") return SquareType::IncomeTax;
  if (s == "SUPER_TAX") return SquareType::SuperTax;
  if (s == "CHANCE") return SquareType::Chance;
  if (s == "COMMUNITY_CHEST") return SquareType::CommunityChest;
  if (s == "JAIL") return SquareType::Jail;
  if (s == "GO_TO_JAIL") return SquareType::GoToJail;
  if (s == "FREE_PARKING") return SquareType::FreeParking;
  return SquareType::Go;
}

}  // namespace monopoly::domain
