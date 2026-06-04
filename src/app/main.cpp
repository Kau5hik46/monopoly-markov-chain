#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "probability/probability_engine.h"

using namespace monopoly;

namespace {

std::vector<int> rankByProbability(const probability::PositionVector& v) {
  std::vector<int> order(probability::kNumPositions);
  for (int i = 0; i < probability::kNumPositions; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return v[a] > v[b]; });
  return order;
}

void printRow(const domain::Board& board, int pos, double prob) {
  std::cout << "  " << std::setw(2) << pos << "  " << std::setw(24) << std::left
            << board.at(pos).name << std::right << std::setw(7) << prob * 100
            << "%\n";
}

}  // namespace

int main() {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::ProbabilityEngine eng(board, decks);

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "Monopoly Markov Advisor — London edition\n";
  std::cout << "=========================================\n\n";

  auto stat = eng.stationaryByPosition();
  double jail = eng.jailProbability();
  std::cout << "Long-run landing probability (top 10 squares):\n";
  std::cout << "  --  " << std::setw(24) << std::left << "JAIL (in jail)"
            << std::right << std::setw(7) << jail * 100 << "%\n";
  auto order = rankByProbability(stat);
  for (int k = 0; k < 10; ++k) printRow(board, order[k], stat[order[k]]);

  std::cout << "\nNext-roll landing distribution from TRAFALGAR SQUARE (24), top 5:\n";
  auto roll = eng.singleRoll(24);
  auto ro = rankByProbability(roll);
  for (int k = 0; k < 5; ++k) printRow(board, ro[k], roll[ro[k]]);

  return 0;
}
