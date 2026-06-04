#pragma once
#include <array>
#include <utility>
#include <vector>
#include "domain/square.h"

namespace monopoly::domain {

inline constexpr int kBoardSize = 40;

class Board {
 public:
  explicit Board(std::array<Square, kBoardSize> squares)
      : squares_(std::move(squares)) {}

  const Square& at(int position) const { return squares_[position % kBoardSize]; }
  const std::array<Square, kBoardSize>& squares() const { return squares_; }

  // Positions whose type matches t.
  std::vector<int> positionsOfType(SquareType t) const;
  // Positions in a color group (for monopoly checks).
  std::vector<int> positionsInGroup(ColorGroup g) const;
  // Nearest position (searching forward, wrapping) of type t from `from`.
  int nearestForward(int from, SquareType t) const;

 private:
  std::array<Square, kBoardSize> squares_;
};

}  // namespace monopoly::domain
