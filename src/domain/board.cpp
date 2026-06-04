#include "domain/board.h"

namespace monopoly::domain {

std::vector<int> Board::positionsOfType(SquareType t) const {
  std::vector<int> out;
  for (const auto& s : squares_)
    if (s.type == t) out.push_back(s.position);
  return out;
}

std::vector<int> Board::positionsInGroup(ColorGroup g) const {
  std::vector<int> out;
  for (const auto& s : squares_)
    if (s.group == g) out.push_back(s.position);
  return out;
}

int Board::nearestForward(int from, SquareType t) const {
  for (int step = 1; step <= kBoardSize; ++step) {
    int p = (from + step) % kBoardSize;
    if (squares_[p].type == t) return p;
  }
  return from;
}

}  // namespace monopoly::domain
