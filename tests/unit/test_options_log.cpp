#include <gtest/gtest.h>
#include <sstream>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "engine/repl.h"
#include "rules/rule_config.h"

using namespace monopoly;

TEST(OptionsLog, LogReplaysCommandJournal) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  rules::RuleConfig rules;
  std::ostringstream out;
  engine::Repl repl(board, decks, rules, out);

  std::istringstream in("init 2\nbuy P1 @#1\nlog\nquit\n");
  repl.run(in);

  const std::string s = out.str();
  EXPECT_NE(s.find("init 2"), std::string::npos);
  EXPECT_NE(s.find("buy P1"), std::string::npos);
}
