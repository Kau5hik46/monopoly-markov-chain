#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/amount.h"
#include "engine/name_table.h"

using namespace monopoly::engine;

TEST(Amount, ParsesKAndM) {
  EXPECT_EQ(parseAmount("500000").value(), 500000);
  EXPECT_EQ(parseAmount("500K").value(), 500000);
  EXPECT_EQ(parseAmount("2M").value(), 2000000);
  EXPECT_EQ(parseAmount("1.5M").value(), 1500000);
  EXPECT_FALSE(parseAmount("abc").has_value());
  EXPECT_FALSE(parseAmount("12X").has_value());
}

TEST(Amount, FormatsKAndM) {
  EXPECT_EQ(formatMoney(1610000), "\xC2\xA3" "1.61M");
  EXPECT_EQ(formatMoney(250000), "\xC2\xA3" "250.0K");
  EXPECT_EQ(formatMoney(48), "\xC2\xA3" "48");
}

TEST(NameTable, ResolvesNameAndPosition) {
  auto board = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  NameTable nt(board);
  EXPECT_EQ(nt.resolve("#24").value(), 24);
  EXPECT_EQ(nt.resolve("24").value(), 24);
  EXPECT_EQ(nt.resolve("TRAFALGAR_SQUARE").value(), 24);
  EXPECT_EQ(nt.resolve("the_city").value(), 39);
  EXPECT_FALSE(nt.resolve("NOWHERE").has_value());
  EXPECT_FALSE(nt.resolve("#99").has_value());
}
