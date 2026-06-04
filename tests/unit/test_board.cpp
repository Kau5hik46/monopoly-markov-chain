#include <gtest/gtest.h>
#include <vector>
#include "domain/color_group.h"
#include "domain/square.h"
#include "domain/board.h"
#include "domain/board_factory.h"

using namespace monopoly::domain;

#ifndef BOARD_JSON_PATH
#define BOARD_JSON_PATH "data/board.london.json"
#endif

TEST(Square, ClassifiesPurchasable) {
  Square street{1, "PORTOBELLO ROAD MARKET", SquareType::Street, ColorGroup::Brown,
                600000, 500000, 300000};
  Square go{0, "GO", SquareType::Go, ColorGroup::None, 0, 0, 0};
  EXPECT_TRUE(isPurchasable(street.type));
  EXPECT_FALSE(isPurchasable(go.type));
}

TEST(Square, ColorGroupNameRoundTrips) {
  EXPECT_EQ(colorGroupFromString("BROWN"), ColorGroup::Brown);
  EXPECT_EQ(colorGroupFromString("STATION"), ColorGroup::Station);
  EXPECT_EQ(colorGroupFromString("NONE"), ColorGroup::None);
}

TEST(Board, LoadsFortySquares) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  EXPECT_EQ(board.squares().size(), 40u);
  EXPECT_EQ(board.at(0).name, "GO");
  EXPECT_EQ(board.at(39).name, "THE CITY");
  EXPECT_EQ(board.at(10).type, SquareType::Jail);
  EXPECT_EQ(board.at(30).type, SquareType::GoToJail);
}

TEST(Board, FindsCardAndStationSquares) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  EXPECT_EQ(board.positionsOfType(SquareType::Chance),
            (std::vector<int>{7, 22, 36}));
  EXPECT_EQ(board.positionsOfType(SquareType::Station),
            (std::vector<int>{5, 15, 25, 35}));
  // nearest station forward from a Chance square at 7 is 15.
  EXPECT_EQ(board.nearestForward(7, SquareType::Station), 15);
}
