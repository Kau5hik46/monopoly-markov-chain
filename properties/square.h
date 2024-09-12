#include <string>
#include <map>
#include <list>

#include "../players/player.h"


enum ColorGroup
{
    // Monopoly color groups

    BROWN,
    LIGHT_BLUE,
    PURPLE,
    ORANGE,
    RED,
    YELLOW, 
    GREEN,
    BLUE,
    UTILITY,
    TRANSPORTS,
    CHANCE,
    COMMUNITY_CHEST,
    JAIL,
    GO_JAIL,
    GO,
    INCOME_TAX,
    LUXURY_TAX,
    FREE_PARKING
};

enum Buildings{
    SITE_ONLY = 0,
    ONE_HOUSE = 1,
    TWO_HOUSES = 2,
    THREE_HOUSES = 3,
    FOUR_HOUSES = 4,
    HOTEL = 5
};

class Square;