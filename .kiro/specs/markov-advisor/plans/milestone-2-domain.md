# Milestone 2: Domain — Board & Cards — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Model the London board geometry and the Chance/Community-Chest movement decks, loaded from JSON config, so the M3 Markov engine has the squares and card-branching data it needs.

**Architecture:** `monopoly::domain` namespace. `ColorGroup` + `SquareType` enums, a flat `Square` value type, a `Board` (`std::array<Square,40>` + lookups), a `BoardFactory` that parses `data/board.london.json`, and a `CardDeck`/`DeckFactory` that parses `data/decks.london.json`. JSON via header-only nlohmann/json (FetchContent). Visitor over square types is deferred to M4 (rent), where polymorphism pays off; M2 uses an enum tag.

**Tech Stack:** C++17, nlohmann/json, GoogleTest.

> **Scope note (roadmap adjustment):** live-state entities (`Player`, `Bank`,
> `Account`, `Asset`, `GameState`) are pulled into a new milestone **M2.5** just
> before M4, since ownership first matters at the risk layer. M2 here = board + cards.

> **Card-deck data note:** the themed board ships no official card list. We encode the
> canonical Monopoly *movement* set (10 Chance movers, 2 Community-Chest movers)
> mapped onto this board, documented in `data/decks.london.json`. Non-movement
> (money) cards are represented as a single `NONE` weight so chain branching is exact.

---

## File structure (this milestone)

- Create: `src/domain/color_group.h` — `ColorGroup` enum + group/position helpers.
- Create: `src/domain/square.h` — `SquareType`, `Square` value type.
- Create: `src/domain/board.h` / `board.cpp` — `Board` container + lookups.
- Create: `src/domain/board_factory.h` / `board_factory.cpp` — JSON → `Board`.
- Create: `src/domain/card.h` — `CardEffect`, `CardDeck`.
- Create: `src/domain/deck_factory.h` / `deck_factory.cpp` — JSON → decks.
- Create: `data/board.london.json`, `data/decks.london.json`.
- Modify: `CMakeLists.txt` — add nlohmann/json, new sources.
- Create: `tests/unit/test_board.cpp`, `tests/unit/test_deck.cpp`.

---

### Task 1: ColorGroup and Square value types (header-only)

**Files:**
- Create: `src/domain/color_group.h`
- Create: `src/domain/square.h`
- Create: `tests/unit/test_board.cpp`
- Modify: `CMakeLists.txt` (add test file)

- [ ] **Step 1: Write the failing test** (`tests/unit/test_board.cpp`)

```cpp
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
```

- [ ] **Step 2: Add the test to CMake and verify it fails**

In `CMakeLists.txt`, add `tests/unit/test_board.cpp` to the `monopoly_tests` sources.
Run: `cmake -S . -B build && cmake --build build -j 2>&1 | grep -E "error" | head`
Expected: errors — `color_group.h` not found.

- [ ] **Step 3: Implement `src/domain/color_group.h`**

```cpp
#pragma once
#include <string>
#include <string_view>

namespace monopoly::domain {

enum class ColorGroup {
  None, Brown, LightBlue, Pink, Orange, Red, Yellow, Green, DarkBlue,
  Station, Utility
};

inline ColorGroup colorGroupFromString(std::string_view s) {
  if (s == "BROWN") return ColorGroup::Brown;
  if (s == "LIGHT_BLUE") return ColorGroup::LightBlue;
  if (s == "PINK") return ColorGroup::Pink;
  if (s == "ORANGE") return ColorGroup::Orange;
  if (s == "RED") return ColorGroup::Red;
  if (s == "YELLOW") return ColorGroup::Yellow;
  if (s == "GREEN") return ColorGroup::Green;
  if (s == "DARK_BLUE") return ColorGroup::DarkBlue;
  if (s == "STATION") return ColorGroup::Station;
  if (s == "UTILITY") return ColorGroup::Utility;
  return ColorGroup::None;
}

}  // namespace monopoly::domain
```

- [ ] **Step 4: Implement `src/domain/square.h`**

```cpp
#pragma once
#include <string>
#include "domain/color_group.h"

namespace monopoly::domain {

enum class SquareType {
  Go, Street, Station, Utility, IncomeTax, SuperTax,
  Chance, CommunityChest, Jail, GoToJail, FreeParking
};

// Flat value type for a board square. Purchase fields are 0 for non-property squares.
struct Square {
  int position = 0;
  std::string name;
  SquareType type = SquareType::Go;
  ColorGroup group = ColorGroup::None;
  long price = 0;       // listed purchase price
  long houseCost = 0;   // per-house build cost (streets only)
  long mortgage = 0;    // mortgage value
};

inline bool isPurchasable(SquareType t) {
  return t == SquareType::Street || t == SquareType::Station ||
         t == SquareType::Utility;
}

inline SquareType squareTypeFromString(std::string_view s) {
  if (s == "GO") return SquareType::Go;
  if (s == "STREET") return SquareType::Street;
  if (s == "STATION") return SquareType::Station;
  if (s == "UTILITY") return SquareType::Utility;
  if (s == "INCOME_TAX") return SquareType::IncomeTax;
  if (s == "SUPER_TAX") return SquareType::SuperTax;
  if (s == "CHANCE") return SquareType::Chance;
  if (s == "COMMUNITY_CHEST") return SquareType::CommunityChest;
  if (s == "JAIL") return SquareType::Jail;
  if (s == "GO_TO_JAIL") return SquareType::GoToJail;
  if (s == "FREE_PARKING") return SquareType::FreeParking;
  return SquareType::Go;
}

}  // namespace monopoly::domain
```

Note: `square.h` needs `#include <string_view>` (via color_group.h). Add it explicitly.

- [ ] **Step 5: Build and run — verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R Square --output-on-failure`
Expected: `Square.*` tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/domain/color_group.h src/domain/square.h tests/unit/test_board.cpp CMakeLists.txt
git commit -m "feat(domain): ColorGroup and Square value types"
```

---

### Task 2: nlohmann/json dependency + board.london.json + BoardFactory

**Files:**
- Modify: `CMakeLists.txt`
- Create: `data/board.london.json`
- Create: `src/domain/board.h`, `src/domain/board.cpp`
- Create: `src/domain/board_factory.h`, `src/domain/board_factory.cpp`
- Modify: `tests/unit/test_board.cpp`

- [ ] **Step 1: Add nlohmann/json via FetchContent and register new sources**

In `CMakeLists.txt`, after the googletest FetchContent block add:
```cmake
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
```
Add `src/domain/board.cpp` and `src/domain/board_factory.cpp` to `monopoly_core`, and
`target_link_libraries(monopoly_core PUBLIC nlohmann_json::nlohmann_json)`.

- [ ] **Step 2: Write `data/board.london.json`** (full 40-square geometry)

```json
{
  "edition": "london",
  "squares": [
    {"position":0,"name":"GO","type":"GO","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":1,"name":"PORTOBELLO ROAD MARKET","type":"STREET","group":"BROWN","price":600000,"houseCost":500000,"mortgage":300000},
    {"position":2,"name":"COMMUNITY CHEST","type":"COMMUNITY_CHEST","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":3,"name":"CAMDEN MARKET","type":"STREET","group":"BROWN","price":600000,"houseCost":500000,"mortgage":300000},
    {"position":4,"name":"INCOME TAX","type":"INCOME_TAX","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":5,"name":"LONDON CITY AIRPORT","type":"STATION","group":"STATION","price":2000000,"houseCost":0,"mortgage":1000000},
    {"position":6,"name":"HAMMERSMITH APOLLO","type":"STREET","group":"LIGHT_BLUE","price":1000000,"houseCost":500000,"mortgage":500000},
    {"position":7,"name":"CHANCE","type":"CHANCE","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":8,"name":"WEMBLEY ARENA","type":"STREET","group":"LIGHT_BLUE","price":1000000,"houseCost":500000,"mortgage":500000},
    {"position":9,"name":"GMTV STUDIOS","type":"STREET","group":"LIGHT_BLUE","price":1200000,"houseCost":500000,"mortgage":600000},
    {"position":10,"name":"JAIL","type":"JAIL","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":11,"name":"THE OVAL","type":"STREET","group":"PINK","price":1400000,"houseCost":1000000,"mortgage":700000},
    {"position":12,"name":"TELECOMS","type":"UTILITY","group":"UTILITY","price":1500000,"houseCost":0,"mortgage":750000},
    {"position":13,"name":"WIMBLEDON","type":"STREET","group":"PINK","price":1400000,"houseCost":1000000,"mortgage":700000},
    {"position":14,"name":"WEMBLEY STADIUM","type":"STREET","group":"PINK","price":1600000,"houseCost":1000000,"mortgage":800000},
    {"position":15,"name":"STANSTED AIRPORT","type":"STATION","group":"STATION","price":2000000,"houseCost":0,"mortgage":1000000},
    {"position":16,"name":"SCIENCE MUSEUM","type":"STREET","group":"ORANGE","price":1800000,"houseCost":1000000,"mortgage":900000},
    {"position":17,"name":"COMMUNITY CHEST","type":"COMMUNITY_CHEST","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":18,"name":"NATURAL HISTORY MUSEUM","type":"STREET","group":"ORANGE","price":1800000,"houseCost":1000000,"mortgage":900000},
    {"position":19,"name":"TATE MODERN","type":"STREET","group":"ORANGE","price":2000000,"houseCost":1000000,"mortgage":1000000},
    {"position":20,"name":"FREE PARKING","type":"FREE_PARKING","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":21,"name":"LONDON EYE","type":"STREET","group":"RED","price":2200000,"houseCost":1500000,"mortgage":1100000},
    {"position":22,"name":"CHANCE","type":"CHANCE","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":23,"name":"HYDE PARK","type":"STREET","group":"RED","price":2200000,"houseCost":1500000,"mortgage":1100000},
    {"position":24,"name":"TRAFALGAR SQUARE","type":"STREET","group":"RED","price":2400000,"houseCost":1500000,"mortgage":1200000},
    {"position":25,"name":"GATWICK AIRPORT","type":"STATION","group":"STATION","price":2000000,"houseCost":0,"mortgage":1000000},
    {"position":26,"name":"TOTTENHAM COURT ROAD","type":"STREET","group":"YELLOW","price":2600000,"houseCost":1500000,"mortgage":1300000},
    {"position":27,"name":"COVENT GARDEN","type":"STREET","group":"YELLOW","price":2600000,"houseCost":1500000,"mortgage":1300000},
    {"position":28,"name":"THE SUN","type":"UTILITY","group":"UTILITY","price":1500000,"houseCost":0,"mortgage":750000},
    {"position":29,"name":"REGENT STREET","type":"STREET","group":"YELLOW","price":2800000,"houseCost":1500000,"mortgage":1400000},
    {"position":30,"name":"GO TO JAIL","type":"GO_TO_JAIL","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":31,"name":"NOTTING HILL","type":"STREET","group":"GREEN","price":3000000,"houseCost":2000000,"mortgage":1500000},
    {"position":32,"name":"SOHO","type":"STREET","group":"GREEN","price":3000000,"houseCost":2000000,"mortgage":1500000},
    {"position":33,"name":"COMMUNITY CHEST","type":"COMMUNITY_CHEST","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":34,"name":"KINGS ROAD","type":"STREET","group":"GREEN","price":3200000,"houseCost":2000000,"mortgage":1600000},
    {"position":35,"name":"HEATHROW AIRPORT","type":"STATION","group":"STATION","price":2000000,"houseCost":0,"mortgage":1000000},
    {"position":36,"name":"CHANCE","type":"CHANCE","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":37,"name":"CANARY WHARF","type":"STREET","group":"DARK_BLUE","price":3500000,"houseCost":2000000,"mortgage":1750000},
    {"position":38,"name":"SUPER TAX","type":"SUPER_TAX","group":"NONE","price":0,"houseCost":0,"mortgage":0},
    {"position":39,"name":"THE CITY","type":"STREET","group":"DARK_BLUE","price":4000000,"houseCost":2000000,"mortgage":2000000}
  ]
}
```

- [ ] **Step 3: Implement `src/domain/board.h`**

```cpp
#pragma once
#include <array>
#include <vector>
#include "domain/square.h"

namespace monopoly::domain {

inline constexpr int kBoardSize = 40;

class Board {
 public:
  explicit Board(std::array<Square, kBoardSize> squares)
      : squares_(std::move(squares)) {}

  const Square& at(int position) const { return squares_[position % kBoardSize]; }
  const std::array<Square, kBoardSize>& squares() const { return squares_; }

  // Positions whose type matches t.
  std::vector<int> positionsOfType(SquareType t) const;
  // Positions in a color group (for monopoly checks).
  std::vector<int> positionsInGroup(ColorGroup g) const;
  // Nearest position (searching forward, wrapping) of type t from `from`.
  int nearestForward(int from, SquareType t) const;

 private:
  std::array<Square, kBoardSize> squares_;
};

}  // namespace monopoly::domain
```

- [ ] **Step 4: Implement `src/domain/board.cpp`**

```cpp
#include "domain/board.h"

namespace monopoly::domain {

std::vector<int> Board::positionsOfType(SquareType t) const {
  std::vector<int> out;
  for (const auto& s : squares_)
    if (s.type == t) out.push_back(s.position);
  return out;
}

std::vector<int> Board::positionsInGroup(ColorGroup g) const {
  std::vector<int> out;
  for (const auto& s : squares_)
    if (s.group == g) out.push_back(s.position);
  return out;
}

int Board::nearestForward(int from, SquareType t) const {
  for (int step = 1; step <= kBoardSize; ++step) {
    int p = (from + step) % kBoardSize;
    if (squares_[p].type == t) return p;
  }
  return from;
}

}  // namespace monopoly::domain
```

- [ ] **Step 5: Implement `src/domain/board_factory.h`**

```cpp
#pragma once
#include <string>
#include "domain/board.h"

namespace monopoly::domain {

// Loads a Board from a JSON file shaped like data/board.london.json.
// Throws std::runtime_error on malformed input or wrong square count.
Board loadBoardFromFile(const std::string& path);

}  // namespace monopoly::domain
```

- [ ] **Step 6: Implement `src/domain/board_factory.cpp`**

```cpp
#include "domain/board_factory.h"

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace monopoly::domain {

Board loadBoardFromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open board file: " + path);
  nlohmann::json j;
  in >> j;

  const auto& arr = j.at("squares");
  if (arr.size() != static_cast<std::size_t>(kBoardSize))
    throw std::runtime_error("board must have exactly 40 squares");

  std::array<Square, kBoardSize> squares{};
  for (const auto& e : arr) {
    Square s;
    s.position = e.at("position").get<int>();
    s.name = e.at("name").get<std::string>();
    s.type = squareTypeFromString(e.at("type").get<std::string>());
    s.group = colorGroupFromString(e.at("group").get<std::string>());
    s.price = e.at("price").get<long>();
    s.houseCost = e.at("houseCost").get<long>();
    s.mortgage = e.at("mortgage").get<long>();
    if (s.position < 0 || s.position >= kBoardSize)
      throw std::runtime_error("square position out of range");
    squares[static_cast<std::size_t>(s.position)] = s;
  }
  return Board(std::move(squares));
}

}  // namespace monopoly::domain
```

- [ ] **Step 7: Write board-loading tests**

Append to `tests/unit/test_board.cpp`:
```cpp
#include "domain/board.h"
#include "domain/board_factory.h"

// Resolved at configure time via a compile definition (see CMake step).
#ifndef BOARD_JSON_PATH
#define BOARD_JSON_PATH "data/board.london.json"
#endif

using monopoly::domain::loadBoardFromFile;

TEST(Board, LoadsFortySquares) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  EXPECT_EQ(board.squares().size(), 40u);
  EXPECT_EQ(board.at(0).name, "GO");
  EXPECT_EQ(board.at(39).name, "THE CITY");
  EXPECT_EQ(board.at(10).type, monopoly::domain::SquareType::Jail);
  EXPECT_EQ(board.at(30).type, monopoly::domain::SquareType::GoToJail);
}

TEST(Board, FindsCardAndStationSquares) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  EXPECT_EQ(board.positionsOfType(monopoly::domain::SquareType::Chance),
            (std::vector<int>{7, 22, 36}));
  EXPECT_EQ(board.positionsOfType(monopoly::domain::SquareType::Station),
            (std::vector<int>{5, 15, 25, 35}));
  // nearest station forward from a Chance square at 7 is 15.
  EXPECT_EQ(board.nearestForward(7, monopoly::domain::SquareType::Station), 15);
}
```

In `CMakeLists.txt` pass the data path to the test target:
```cmake
target_compile_definitions(monopoly_tests PRIVATE
  BOARD_JSON_PATH="${CMAKE_SOURCE_DIR}/data/board.london.json")
```

- [ ] **Step 8: Build and run — verify pass**

Run: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build -R Board --output-on-failure`
Expected: all `Board.*` tests PASS.

- [ ] **Step 9: Commit**

```bash
git add CMakeLists.txt data/board.london.json src/domain/board.h src/domain/board.cpp \
        src/domain/board_factory.h src/domain/board_factory.cpp tests/unit/test_board.cpp
git commit -m "feat(domain): London board JSON + Board container + factory"
```

---

### Task 3: Card decks + DeckFactory

**Files:**
- Create: `src/domain/card.h`
- Create: `src/domain/deck_factory.h`, `src/domain/deck_factory.cpp`
- Create: `data/decks.london.json`
- Modify: `CMakeLists.txt`
- Create: `tests/unit/test_deck.cpp`

- [ ] **Step 1: Implement `src/domain/card.h`**

```cpp
#pragma once
#include <vector>

namespace monopoly::domain {

enum class CardMove {
  None,          // money/other card; no movement
  AdvanceTo,     // go to target position
  GoToJail,      // straight to jail (in-jail)
  NearestStation,
  NearestUtility,
  Back3
};

struct CardEffect {
  CardMove move = CardMove::None;
  int target = 0;   // used by AdvanceTo
  int weight = 1;   // number of cards in the deck with this effect
};

// A deck is a weighted list of effects; weights sum to the deck size.
struct CardDeck {
  std::vector<CardEffect> effects;
  int size() const {
    int n = 0;
    for (const auto& e : effects) n += e.weight;
    return n;
  }
};

}  // namespace monopoly::domain
```

- [ ] **Step 2: Write `data/decks.london.json`**

Canonical Monopoly movement set mapped to the London board. Targets:
GO=0, THE OVAL=11 (St-Charles analog), TRAFALGAR SQUARE=24 (Illinois analog),
LONDON CITY AIRPORT=5 (Reading analog), THE CITY=39 (Boardwalk analog).
```json
{
  "edition": "london",
  "chance": [
    {"move":"ADVANCE_TO","target":0,"weight":1},
    {"move":"ADVANCE_TO","target":24,"weight":1},
    {"move":"ADVANCE_TO","target":11,"weight":1},
    {"move":"ADVANCE_TO","target":5,"weight":1},
    {"move":"ADVANCE_TO","target":39,"weight":1},
    {"move":"NEAREST_STATION","target":0,"weight":2},
    {"move":"NEAREST_UTILITY","target":0,"weight":1},
    {"move":"GO_TO_JAIL","target":0,"weight":1},
    {"move":"BACK_3","target":0,"weight":1},
    {"move":"NONE","target":0,"weight":6}
  ],
  "community_chest": [
    {"move":"ADVANCE_TO","target":0,"weight":1},
    {"move":"GO_TO_JAIL","target":0,"weight":1},
    {"move":"NONE","target":0,"weight":14}
  ]
}
```

- [ ] **Step 3: Implement `src/domain/deck_factory.h`**

```cpp
#pragma once
#include <string>
#include "domain/card.h"

namespace monopoly::domain {

struct Decks {
  CardDeck chance;
  CardDeck communityChest;
};

Decks loadDecksFromFile(const std::string& path);

}  // namespace monopoly::domain
```

- [ ] **Step 4: Implement `src/domain/deck_factory.cpp`**

```cpp
#include "domain/deck_factory.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

namespace monopoly::domain {

namespace {
CardMove moveFromString(const std::string& s) {
  if (s == "NONE") return CardMove::None;
  if (s == "ADVANCE_TO") return CardMove::AdvanceTo;
  if (s == "GO_TO_JAIL") return CardMove::GoToJail;
  if (s == "NEAREST_STATION") return CardMove::NearestStation;
  if (s == "NEAREST_UTILITY") return CardMove::NearestUtility;
  if (s == "BACK_3") return CardMove::Back3;
  throw std::runtime_error("unknown card move: " + s);
}

CardDeck parseDeck(const nlohmann::json& arr) {
  CardDeck deck;
  for (const auto& e : arr) {
    CardEffect c;
    c.move = moveFromString(e.at("move").get<std::string>());
    c.target = e.at("target").get<int>();
    c.weight = e.at("weight").get<int>();
    deck.effects.push_back(c);
  }
  return deck;
}
}  // namespace

Decks loadDecksFromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open decks file: " + path);
  nlohmann::json j;
  in >> j;
  Decks d;
  d.chance = parseDeck(j.at("chance"));
  d.communityChest = parseDeck(j.at("community_chest"));
  return d;
}

}  // namespace monopoly::domain
```

- [ ] **Step 5: Register sources/defs in CMake and write tests**

In `CMakeLists.txt`: add `src/domain/deck_factory.cpp` to `monopoly_core`,
`tests/unit/test_deck.cpp` to `monopoly_tests`, and:
```cmake
target_compile_definitions(monopoly_tests PRIVATE
  DECKS_JSON_PATH="${CMAKE_SOURCE_DIR}/data/decks.london.json")
```

`tests/unit/test_deck.cpp`:
```cpp
#include <gtest/gtest.h>
#include "domain/deck_factory.h"

#ifndef DECKS_JSON_PATH
#define DECKS_JSON_PATH "data/decks.london.json"
#endif

using namespace monopoly::domain;

TEST(Deck, ChanceHasSixteenCards) {
  auto decks = loadDecksFromFile(DECKS_JSON_PATH);
  EXPECT_EQ(decks.chance.size(), 16);
}

TEST(Deck, CommunityChestHasSixteenCards) {
  auto decks = loadDecksFromFile(DECKS_JSON_PATH);
  EXPECT_EQ(decks.communityChest.size(), 16);
}

TEST(Deck, ChanceMovementWeightsAreCanonical) {
  auto decks = loadDecksFromFile(DECKS_JSON_PATH);
  int movers = 0;
  for (const auto& e : decks.chance.effects)
    if (e.move != CardMove::None) movers += e.weight;
  EXPECT_EQ(movers, 10);  // 10 movement cards, 6 money cards
}
```

- [ ] **Step 6: Build and run — verify pass**

Run: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build -R Deck --output-on-failure`
Expected: all `Deck.*` tests PASS.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt data/decks.london.json src/domain/card.h \
        src/domain/deck_factory.h src/domain/deck_factory.cpp tests/unit/test_deck.cpp
git commit -m "feat(domain): card decks (canonical movement set) + deck factory"
```

---

## Milestone 2 acceptance
- `ctest` green; Board loads 40 squares with correct types/groups; card squares at
  7/22/36 and stations at 5/15/25/35; decks total 16 each with 10/2 movers.
- All domain files < 400 LoC; `monopoly_core` links nlohmann/json.
