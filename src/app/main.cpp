#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "pricing/insurance_pricer.h"
#include "probability/landing.h"
#include "probability/probability_engine.h"

using namespace monopoly;

namespace {

// Data directory: argv[1] > $MONOPOLY_DATA_DIR > compile-time default.
std::string resolveDataDir(int argc, char** argv) {
  if (argc > 1) return argv[1];
  if (const char* env = std::getenv("MONOPOLY_DATA_DIR")) return env;
#ifdef MONOPOLY_DATA_DIR
  return MONOPOLY_DATA_DIR;
#else
  return "data";
#endif
}

// Format a money amount in K/M units, e.g. £1.61M, £250.0K, £48.
std::string money(double v) {
  std::ostringstream os;
  const double a = std::fabs(v);
  os << "\xC2\xA3";
  if (a >= 1e6) os << std::fixed << std::setprecision(2) << v / 1e6 << "M";
  else if (a >= 1e3) os << std::fixed << std::setprecision(1) << v / 1e3 << "K";
  else os << std::fixed << std::setprecision(0) << v;
  return os.str();
}

std::vector<int> rankByProbability(const probability::PositionVector& v) {
  std::vector<int> order(probability::kNumPositions);
  for (int i = 0; i < probability::kNumPositions; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return v[a] > v[b]; });
  return order;
}

void printRow(const domain::Board& board, int pos, double prob) {
  std::cout << "  " << std::setw(2) << pos << "  " << std::setw(24) << std::left
            << board.at(pos).name << std::right << std::setw(7) << std::fixed
            << std::setprecision(3) << prob * 100 << "%\n";
}

}  // namespace

int run(int argc, char** argv) {
  const std::string dataDir = resolveDataDir(argc, argv);
  auto board = domain::loadBoardFromFile(dataDir + "/board.london.json");
  auto decks = domain::loadDecksFromFile(dataDir + "/decks.london.json");
  probability::ProbabilityEngine eng(board, decks);
  probability::LandingResolver resolver(board, decks);

  std::cout << "Monopoly Markov Advisor — London edition\n";
  std::cout << "=========================================\n\n";

  auto stat = eng.stationaryByPosition();
  double jail = eng.jailProbability();
  std::cout << "Long-run landing probability (top 10 squares):\n";
  std::cout << "  --  " << std::setw(24) << std::left << "JAIL (in jail)"
            << std::right << std::setw(7) << std::fixed << std::setprecision(3)
            << jail * 100 << "%\n";
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
  gs.player(bob).position = 24;               // BOB also standing on it (muggable)

  pricing::PricingConfig cfg;
  cfg.muggingEnabled = true;
  auto quote = pricing::priceNextRoll(gs, alice, resolver, cfg);

  std::cout << "\nInsurance quote — ALICE about to roll from "
            << board.at(18).name << " (18):\n";
  std::cout << "  expected next-roll rent liability : " << money(quote.expectedRent)
            << "\n";
  std::cout << "  mugging expected value (benefit)  : " << money(quote.muggingExposure)
            << "\n";
  std::cout << "  fair premium to insure this roll  : " << money(quote.fairPremium)
            << "\n";
  return 0;
}

int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n"
              << "usage: monopoly [DATA_DIR]   (or set MONOPOLY_DATA_DIR)\n";
    return 1;
  }
}
