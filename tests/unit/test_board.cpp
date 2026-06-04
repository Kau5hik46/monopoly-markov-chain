#include <gtest/gtest.h>
#include "domain/color_group.h"
#include "domain/square.h"

using namespace monopoly::domain;

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
