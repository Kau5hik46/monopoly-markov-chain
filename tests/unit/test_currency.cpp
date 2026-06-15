#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/amount.h"
#include "rules/rule_config.h"

using namespace monopoly;

TEST(Currency, BoardDefaultsToPound) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  EXPECT_EQ(board.currency(), "\xC2\xA3");  // £
}

TEST(Currency, FormatMoneyUsesActiveSymbol) {
  const std::string saved = engine::moneySymbol();
  engine::moneySymbol() = "$";
  EXPECT_EQ(engine::formatMoney(600000), "$600.0K");
  EXPECT_EQ(engine::formatMoney(2000000), "$2.00M");
  engine::moneySymbol() = saved;  // restore so other suites see the default
  EXPECT_EQ(engine::formatMoney(48), "\xC2\xA3""48");
}

TEST(UsBoard, LiteralDollarValuesAndCurrency) {
  auto board = domain::loadBoardFromFile(US_BOARD_JSON_PATH);
  EXPECT_EQ(board.currency(), "$");
  EXPECT_EQ(board.squares().size(), 40u);
  EXPECT_EQ(board.at(1).name, "MEDITERRANEAN AVENUE");
  EXPECT_EQ(board.at(1).price, 60);        // literal $60, not 600000
  EXPECT_EQ(board.at(1).rent[0], 2);       // site rent $2
  EXPECT_EQ(board.at(39).name, "BOARDWALK");
  EXPECT_EQ(board.at(39).price, 400);
  EXPECT_EQ(board.rentRules().stationRentByCount[0], 25);  // literal station rent
}

TEST(UsBoard, EconomyIsLiteralDollars) {
  auto rules = rules::loadRuleConfig(US_RULES_JSON_PATH);
  EXPECT_EQ(rules.startingCash, 1500);     // $1500, not £15M
  EXPECT_EQ(rules.passGoBonus, 200);       // $200 salary
  EXPECT_EQ(rules.jailFine, 50);           // $50
}
