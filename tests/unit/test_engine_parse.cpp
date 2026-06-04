#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/lexer.h"
#include "engine/parser.h"

using namespace monopoly::engine;

namespace {
NameTable makeNames() {
  static auto board = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  return NameTable(board);
}
Command parse(const std::string& s) { return parseLine(s, makeNames()); }
}  // namespace

TEST(Lexer, SplitsOperators) {
  auto t = lex("roll P1 = 3,4");
  ASSERT_EQ(t.size(), 6u);
  EXPECT_EQ(t[0], "roll");
  EXPECT_EQ(t[2], "=");
  EXPECT_EQ(t[3], "3");
  EXPECT_EQ(t[4], ",");
  EXPECT_EQ(t[5], "4");
}

TEST(Parser, Roll) {
  auto c = parse("roll P1 = 3,4");
  EXPECT_EQ(c.kind, CommandKind::Roll);
  EXPECT_EQ(c.player, 0);
  EXPECT_EQ(c.die1, 3);
  EXPECT_EQ(c.die2, 4);
}

TEST(Parser, BuyWithAndWithoutPrice) {
  auto c1 = parse("buy P2 @TRAFALGAR_SQUARE = 2.4M");
  EXPECT_EQ(c1.kind, CommandKind::Buy);
  EXPECT_EQ(c1.player, 1);
  EXPECT_EQ(c1.posA, 24);
  EXPECT_TRUE(c1.hasAmount);
  EXPECT_EQ(c1.amount, 2400000);
  auto c2 = parse("buy P2 @#24");
  EXPECT_EQ(c2.kind, CommandKind::Buy);
  EXPECT_FALSE(c2.hasAmount);
}

TEST(Parser, RentTransferAndBuild) {
  auto r = parse("rent P2 -> P1 @THE_OVAL");
  EXPECT_EQ(r.kind, CommandKind::Rent);
  EXPECT_EQ(r.player, 1);
  EXPECT_EQ(r.player2, 0);
  EXPECT_EQ(r.posA, 11);
  auto b = parse("build P1 @SOHO + 2");
  EXPECT_EQ(b.kind, CommandKind::Build);
  EXPECT_TRUE(b.sign);
  EXPECT_EQ(b.count, 2);
}

TEST(Parser, MugAndAirportAndCash) {
  auto m = parse("mug P1 vs P2 = 9:5");
  EXPECT_EQ(m.kind, CommandKind::Mug);
  EXPECT_EQ(m.die1, 9);
  EXPECT_EQ(m.die3, 5);
  auto a = parse("airport P1 @#5 -> @#15");
  EXPECT_EQ(a.kind, CommandKind::Airport);
  EXPECT_EQ(a.posA, 5);
  EXPECT_EQ(a.posB, 15);
  auto cash = parse("cash P1 += 500K");
  EXPECT_EQ(cash.kind, CommandKind::Cash);
  EXPECT_TRUE(cash.sign);
  EXPECT_EQ(cash.amount, 500000);
}

TEST(Parser, QueryForms) {
  EXPECT_EQ(parse("query stationary").query, QueryKind::Stationary);
  auto d = parse("query dist P1 ^5");
  EXPECT_EQ(d.kind, CommandKind::Query);
  EXPECT_EQ(d.query, QueryKind::Dist);
  EXPECT_EQ(d.count, 5);
  EXPECT_EQ(parse("query value @#39").posA, 39);
}

TEST(Parser, Errors) {
  EXPECT_EQ(parse("roll P1 = 7,4").kind, CommandKind::Invalid);  // die > 6
  EXPECT_EQ(parse("buy P1 @NOWHERE").kind, CommandKind::Invalid);
  EXPECT_EQ(parse("frobnicate X").kind, CommandKind::Invalid);
  EXPECT_EQ(parse("").kind, CommandKind::None);
}
