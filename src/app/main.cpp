#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "pricing/insurance_pricer.h"
#include "probability/landing.h"
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
  probability::LandingResolver resolver(board, decks);

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

  // --- Insurance / option pricing demo (requirement A) ---
  domain::GameState gs(board);
  int alice = gs.addPlayer("ALICE", 15000000);
  int bob = gs.addPlayer("BOB", 15000000);
  gs.player(alice).position = 18;            // about to roll
  gs.setOwner(24, bob);                       // BOB owns the RED set...
  gs.setOwner(21, bob);
  gs.setOwner(23, bob);
  gs.setHouses(24, 5);                        // ...with a hotel on Trafalgar Square

  pricing::PricingConfig cfg;
  cfg.muggingEnabled = true;
  gs.player(bob).position = 24;               // BOB also standing on it (muggable)
  auto quote = pricing::priceNextRoll(gs, alice, resolver, cfg);

  std::cout << "\nInsurance quote — ALICE about to roll from "
            << board.at(18).name << " (18):\n";
  std::cout << "  expected next-roll rent liability : \xC2\xA3" << std::setprecision(0)
            << quote.expectedRent << "\n";
  std::cout << "  mugging expected value (benefit)  : \xC2\xA3" << quote.muggingExposure
            << "\n";
  std::cout << "  fair premium to insure this roll  : \xC2\xA3" << quote.fairPremium
            << "\n";
  return 0;
}
