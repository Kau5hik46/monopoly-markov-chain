#pragma once
#include <string>
#include <string_view>

namespace monopoly::domain {

enum class ColorGroup {
  None, Brown, LightBlue, Pink, Orange, Red, Yellow, Green, DarkBlue,
  Station, Utility
};

inline ColorGroup colorGroupFromString(std::string_view s) {
  if (s == "BROWN") return ColorGroup::Brown;
  if (s == "LIGHT_BLUE") return ColorGroup::LightBlue;
  if (s == "PINK") return ColorGroup::Pink;
  if (s == "ORANGE") return ColorGroup::Orange;
  if (s == "RED") return ColorGroup::Red;
  if (s == "YELLOW") return ColorGroup::Yellow;
  if (s == "GREEN") return ColorGroup::Green;
  if (s == "DARK_BLUE") return ColorGroup::DarkBlue;
  if (s == "STATION") return ColorGroup::Station;
  if (s == "UTILITY") return ColorGroup::Utility;
  return ColorGroup::None;
}

}  // namespace monopoly::domain
