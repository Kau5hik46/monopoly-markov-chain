#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/completion.h"
#include "engine/name_table.h"

using namespace monopoly::engine;
using monopoly::domain::loadBoardFromFile;

namespace {
CompletionModel model(int players, std::vector<std::string> actions, const NameTable& nt) {
  return makeCompletionModel(nt, players, std::move(actions));
}
}  // namespace

TEST(Completion, CleanPromptStripsTails) {
  EXPECT_EQ(cleanPrompt("buy P1 @#1   (Old Kent Road, \xC2\xA3""600K)  \xE2\x80\x94 or skip"),
            "buy P1 @#1");
  EXPECT_EQ(cleanPrompt("settle all"), "settle all");
  EXPECT_EQ(cleanPrompt("tax P1 = <amount> @INCOME"), "tax P1 = <amount> @INCOME");
  EXPECT_EQ(cleanPrompt("claim P1   (free-parking pot: 2 houses)"), "claim P1");
}

TEST(Completion, EmptyBufferCyclesRecommendedActions) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {"buy P1 @#1", "settle all"}, nt);
  auto a = complete("", m, 0);
  ASSERT_TRUE(a.replaced); EXPECT_EQ(a.replacement, "buy P1 @#1");
  auto b = complete("", m, 1);
  ASSERT_TRUE(b.replaced); EXPECT_EQ(b.replacement, "settle all");
  auto c = complete("", m, 2);  // wraps
  ASSERT_TRUE(c.replaced); EXPECT_EQ(c.replacement, "buy P1 @#1");
  auto back = complete("", m, -1);  // Shift-TAB wraps backward
  ASSERT_TRUE(back.replaced); EXPECT_EQ(back.replacement, "settle all");
}

TEST(Completion, EmptyBufferNoActionsListsVerbs) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {}, nt);
  auto r = complete("", m, 0);
  EXPECT_FALSE(r.replaced);
  EXPECT_FALSE(r.candidates.empty());
}

TEST(Completion, VerbPrefixCompletes) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {}, nt);
  auto single = complete("insu", m, 0);
  ASSERT_TRUE(single.replaced); EXPECT_EQ(single.replacement, "insure");
  auto multi = complete("in", m, 0);   // init, insure
  EXPECT_FALSE(multi.replaced);
  EXPECT_GE(multi.candidates.size(), 2u);
}

TEST(Completion, QuerySubcommandsAndOperands) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(3, {}, nt);
  auto led = complete("query le", m, 0);
  ASSERT_TRUE(led.replaced); EXPECT_EQ(led.replacement, "query ledger");
  auto players = complete("buy P", m, 0);
  EXPECT_EQ(players.candidates.size(), 3u);   // P1 P2 P3
  auto kw = complete("write P2 -> P1 st", m, 0);
  ASSERT_TRUE(kw.replaced); EXPECT_EQ(kw.replacement, "write P2 -> P1 strike");
}

TEST(Completion, AtSquareDelegatesToNameTable) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  auto m = model(2, {}, nt);
  auto r = complete("buy P1 @PORT", m, 0);  // PORTOBELLO ROAD MARKET on the London board
  EXPECT_TRUE(r.replaced || !r.candidates.empty());
}
