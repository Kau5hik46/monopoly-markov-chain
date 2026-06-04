#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "engine/amount.h"
#include "engine/repl.h"
#include "probability/probability_engine.h"

using namespace monopoly;

namespace {

std::string resolveDataDir(const std::vector<std::string>& args) {
  for (const auto& a : args)
    if (!a.empty() && a[0] != '-') return a;
  if (const char* env = std::getenv("MONOPOLY_DATA_DIR")) return env;
#ifdef MONOPOLY_DATA_DIR
  return MONOPOLY_DATA_DIR;
#else
  return "data";
#endif
}

bool hasFlag(const std::vector<std::string>& args, const std::string& f) {
  return std::find(args.begin(), args.end(), f) != args.end();
}

// Quick non-interactive showcase of the probability engine.
void runDemo(const domain::Board& board, const domain::Decks& decks) {
  probability::ProbabilityEngine eng(board, decks);
  auto stat = eng.stationaryByPosition();
  std::vector<int> order(probability::kNumPositions);
  for (int i = 0; i < probability::kNumPositions; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return stat[a] > stat[b]; });
  std::cout << "Long-run landing probability (top 10):\n";
  std::cout << "  JAIL " << eng.jailProbability() * 100 << "%\n";
  for (int k = 0; k < 10; ++k)
    std::cout << "  " << board.at(order[k]).name << " " << stat[order[k]] * 100
              << "%\n";
}

int run(const std::vector<std::string>& args) {
  const std::string dataDir = resolveDataDir(args);
  auto board = domain::loadBoardFromFile(dataDir + "/board.london.json");
  auto decks = domain::loadDecksFromFile(dataDir + "/decks.london.json");

  if (hasFlag(args, "--demo")) {
    runDemo(board, decks);
    return 0;
  }

  auto rules = engine::runRulesWizard(std::cin, std::cout);
  engine::Repl repl(board, decks, rules, std::cout);
  repl.run(std::cin);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  try {
    return run(args);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n"
              << "usage: monopoly [DATA_DIR] [--demo]   (or set MONOPOLY_DATA_DIR)\n";
    return 1;
  }
}
