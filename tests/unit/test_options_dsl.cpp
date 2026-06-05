#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/name_table.h"
#include "engine/parser.h"

using namespace monopoly::engine;

namespace {
NameTable names() {
  auto board = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  return NameTable(board);
}
}  // namespace

TEST(OptionsDsl, InsureSelfDefaultStrikeZero) {
  auto c = parseLine("insure P1", names());
  EXPECT_EQ(c.kind, CommandKind::Insure);
  EXPECT_EQ(c.player, 0);
  EXPECT_EQ(c.insured, 0);     // defaults to holder
  EXPECT_EQ(c.strike, 0);
  EXPECT_FALSE(c.isPut);       // default call
}

TEST(OptionsDsl, InsureWithInsuredAndStrike) {
  auto c = parseLine("insure P1 P3 strike 200K", names());
  EXPECT_EQ(c.kind, CommandKind::Insure);
  EXPECT_EQ(c.player, 0);
  EXPECT_EQ(c.insured, 2);
  EXPECT_TRUE(c.hasStrike);
  EXPECT_GT(c.strike, 0);
}

TEST(OptionsDsl, WritePeerCallWithPremium) {
  auto c = parseLine("write P2 -> P1 P3 strike 100K premium 1.5M", names());
  EXPECT_EQ(c.kind, CommandKind::Write);
  EXPECT_EQ(c.player, 1);    // writer
  EXPECT_EQ(c.player2, 0);   // holder
  EXPECT_EQ(c.insured, 2);
  EXPECT_FALSE(c.isPut);     // default call
  EXPECT_TRUE(c.hasPremium);
}

TEST(OptionsDsl, WritePeerPutAndInsureCall) {
  auto w = parseLine("write P2 -> P1 put strike 200K premium 1M", names());
  EXPECT_EQ(w.kind, CommandKind::Write);
  EXPECT_TRUE(w.isPut);
  EXPECT_EQ(w.insured, 0);   // defaults to holder P1
  auto i = parseLine("insure P1 put strike 200K", names());
  EXPECT_EQ(i.kind, CommandKind::Insure);
  EXPECT_TRUE(i.isPut);
  auto i2 = parseLine("insure P1", names());
  EXPECT_FALSE(i2.isPut);    // default call
}

TEST(OptionsDsl, SettleByIdAndAll) {
  auto a = parseLine("settle 3", names());
  EXPECT_EQ(a.kind, CommandKind::Settle);
  EXPECT_EQ(a.contractId, 3);
  EXPECT_FALSE(a.settleAll);
  auto b = parseLine("settle all", names());
  EXPECT_EQ(b.kind, CommandKind::Settle);
  EXPECT_TRUE(b.settleAll);
}

TEST(OptionsDsl, LogWithAndWithoutCount) {
  EXPECT_EQ(parseLine("log", names()).kind, CommandKind::Log);
  auto c = parseLine("log 20", names());
  EXPECT_EQ(c.kind, CommandKind::Log);
  EXPECT_EQ(c.count, 20);
}

TEST(OptionsDsl, QueryLedger) {
  auto c = parseLine("query ledger", names());
  EXPECT_EQ(c.kind, CommandKind::Query);
  EXPECT_EQ(c.query, QueryKind::Ledger);
}

TEST(OptionsDsl, InsureIncomeCall) {
  auto c = parseLine("insure P1 call income P2 strike 1M", names());
  EXPECT_EQ(c.kind, CommandKind::Insure);
  EXPECT_EQ(c.player, 0);
  EXPECT_TRUE(c.isIncome);
  EXPECT_EQ(c.insured, 1);     // owner P2
  EXPECT_TRUE(c.hasStrike);
}

TEST(OptionsDsl, WriteIncomeWithLanders) {
  auto c = parseLine("write P2 -> P1 call income P3 strike 2M premium 1M landers P3 P4",
                     names());
  EXPECT_EQ(c.kind, CommandKind::Write);
  EXPECT_EQ(c.player, 1);      // writer P2
  EXPECT_EQ(c.player2, 0);     // holder P1
  EXPECT_TRUE(c.isIncome);
  EXPECT_EQ(c.insured, 2);     // owner P3
  ASSERT_EQ(c.landers.size(), 2u);
  EXPECT_EQ(c.landers[0], 2);  // P3
  EXPECT_EQ(c.landers[1], 3);  // P4
}

TEST(OptionsDsl, LiabilityParsingStillWorks) {
  auto c = parseLine("write P2 -> P1 put strike 200K premium 1M", names());
  EXPECT_EQ(c.kind, CommandKind::Write);
  EXPECT_FALSE(c.isIncome);
  EXPECT_TRUE(c.isPut);
  EXPECT_EQ(c.insured, 0);     // defaults to holder
  EXPECT_TRUE(c.landers.empty());
}
