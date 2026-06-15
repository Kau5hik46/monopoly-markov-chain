#pragma once
#include <array>
#include <string>
#include <utility>
#include <vector>
#include "domain/square.h"

namespace monopoly::domain {

inline constexpr int kBoardSize = 40;

// Rent rules that are not per-property data (stations/utilities/monopoly bonus).
// Defaults are the canonical UK Monopoly values (x10000 board scale) and may be
// overridden from the board config file.
struct RentRules {
  std::array<long, 4> stationRentByCount{250000, 500000, 1000000, 2000000};
  long utilityPerPipOne = 40000;    // 4x dice  (x10000)
  long utilityPerPipBoth = 100000;  // 10x dice (x10000)
  long monopolyUndevelopedMultiplier = 2;
};

class Board {
 public:
  explicit Board(std::array<Square, kBoardSize> squares)
      : squares_(std::move(squares)) {}

  const Square& at(int position) const { return squares_[position % kBoardSize]; }
  const std::array<Square, kBoardSize>& squares() const { return squares_; }

  const RentRules& rentRules() const { return rentRules_; }
  void setRentRules(const RentRules& r) { rentRules_ = r; }

  // Display currency symbol (UTF-8), e.g. "£" or "$". Defaults to £.
  const std::string& currency() const { return currency_; }
  void setCurrency(std::string c) { currency_ = std::move(c); }

  // Positions whose type matches t.
  std::vector<int> positionsOfType(SquareType t) const;
  // Positions in a color group (for monopoly checks).
  std::vector<int> positionsInGroup(ColorGroup g) const;
  // Nearest position (searching forward, wrapping) of type t from `from`.
  int nearestForward(int from, SquareType t) const;

 private:
  std::array<Square, kBoardSize> squares_;
  RentRules rentRules_;
  std::string currency_ = "\xC2\xA3";  // £ by default
};

}  // namespace monopoly::domain
