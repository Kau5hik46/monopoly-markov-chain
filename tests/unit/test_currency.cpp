#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/amount.h"

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
