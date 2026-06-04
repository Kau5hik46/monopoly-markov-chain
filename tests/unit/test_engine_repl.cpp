#include <gtest/gtest.h>
#include <sstream>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "engine/repl.h"
#include "rules/rule_config.h"

using namespace monopoly;

TEST(Repl, RunsScriptedSessionWithEffectsAndPanel) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  rules::RuleConfig rules;  // defaults
  std::ostringstream out;
  engine::Repl repl(board, decks, rules, out);

  std::istringstream in(
      "init 2\n"
      "buy P2 @#24\n"
      "roll P1 = 2,4\n"   // P1 from GO... actually starts at 0 -> 6 (Hammersmith)
      "query stationary\n"
      "quit\n");
  repl.run(in);

  const std::string s = out.str();
  EXPECT_NE(s.find("started game with 2 players"), std::string::npos);
  EXPECT_NE(s.find("buys TRAFALGAR SQUARE"), std::string::npos);
  EXPECT_NE(s.find("STATE"), std::string::npos);             // state panel rendered
  EXPECT_NE(s.find("ADVISORY"), std::string::npos);          // advisory rendered
  EXPECT_NE(s.find("TOP THREATS"), std::string::npos);       // threats list
  EXPECT_NE(s.find("long-run landing"), std::string::npos);  // query output
  EXPECT_NE(s.find("bye"), std::string::npos);
}

TEST(Repl, BoardAndHelpCommands) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  rules::RuleConfig rules;
  std::ostringstream out;
  engine::Repl repl(board, decks, rules, out);
  std::istringstream in("init 2\nquery board\nhelp\nquit\n");
  repl.run(in);
  const std::string s = out.str();
  EXPECT_NE(s.find("BOARD"), std::string::npos);
  EXPECT_NE(s.find("TRAFALGAR SQUARE"), std::string::npos);  // board lists squares
  EXPECT_NE(s.find("SYNOPSIS"), std::string::npos);          // man-page help
  EXPECT_NE(s.find("EXAMPLES"), std::string::npos);
}

TEST(Repl, WizardDefaultsOnEmptyInput) {
  std::istringstream in("\n\n\n");  // accept defaults
  std::ostringstream out;
  auto cfg = engine::runRulesWizard(in, out);
  EXPECT_TRUE(cfg.muggingEnabled);
  EXPECT_TRUE(cfg.airportTravelEnabled);
  EXPECT_TRUE(cfg.freeParkingPotEnabled);
}
