#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "engine/amount.h"
#include "engine/repl.h"
#include "engine/style.h"
#include "probability/probability_engine.h"

using namespace monopoly;

namespace {

std::string resolveDataDir(const std::vector<std::string>& args) {
  for (std::size_t i = 0; i < args.size(); ++i) {
    const auto& a = args[i];
    if (a == "--board") { ++i; continue; }  // skip the flag's value (board name)
    if (!a.empty() && a[0] != '-') return a;
  }
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

// Value following `--flag`, or empty string if absent / no value.
std::string flagValue(const std::vector<std::string>& args, const std::string& f) {
  for (std::size_t i = 0; i + 1 < args.size(); ++i)
    if (args[i] == f) return args[i + 1];
  return "";
}

// Resolve the board edition: --board <name>, else an interactive prompt (when stdin is
// a TTY), else the default "london". Accepts london/uk and us/usa (case-insensitive).
std::string chooseBoard(const std::vector<std::string>& args, std::istream& in,
                        std::ostream& out) {
  auto norm = [](std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "uk" || s == "london") return std::string("london");
    if (s == "us" || s == "usa" || s == "america") return std::string("us");
    return s;
  };
  const std::string flag = flagValue(args, "--board");
  if (!flag.empty()) return norm(flag);
  if (!isatty(STDIN_FILENO)) return "london";  // non-interactive default
  out << "Choose a board:  [1] London (UK, £)   [2] US ($)   > ";
  out.flush();
  std::string line;
  if (!std::getline(in, line)) return "london";
  if (line == "2" || norm(line) == "us") return "us";
  return "london";  // default / [1] / blank
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
  const bool demo = hasFlag(args, "--demo");
  // --demo never prompts; otherwise chooseBoard honors --board or asks interactively.
  const std::string edition =
      demo ? (flagValue(args, "--board").empty() ? "london"
                                                  : flagValue(args, "--board"))
           : chooseBoard(args, std::cin, std::cout);
  auto board = domain::loadBoardFromFile(dataDir + "/board." + edition + ".json");
  auto decks = domain::loadDecksFromFile(dataDir + "/decks." + edition + ".json");
  engine::moneySymbol() = board.currency();  // drive £/$ display from the chosen board

  if (demo) {
    runDemo(board, decks);
    return 0;
  }

  engine::Palette pal;
  pal.on = (hasFlag(args, "--color") || isatty(STDOUT_FILENO)) &&
           !hasFlag(args, "--no-color");
  // Per-edition economy base (board-dependent units); fall back to code defaults.
  rules::RuleConfig base;
  try {
    base = rules::loadRuleConfig(dataDir + "/rules." + edition + ".json");
  } catch (const std::exception&) { /* keep defaults */ }
  auto rules = engine::runRulesWizard(std::cin, std::cout, base);
  engine::Repl repl(board, decks, rules, std::cout, pal);
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
              << "usage: monopoly [DATA_DIR] [--board london|us] [--demo]\n"
              << "       (board defaults to an interactive prompt; set MONOPOLY_DATA_DIR "
                 "for data path)\n";
    return 1;
  }
}
