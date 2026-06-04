# Milestone 2.5: Live-State Entities — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Model the mutable, per-turn game ground truth the risk layer reads: player positions/cash/jail status, property ownership, houses, mortgages, the bank's house/hotel supply, and the Free-Parking house pot — with the query helpers M4 needs (monopoly check, station/utility counts, occupancy).

**Architecture:** `monopoly::domain`. `PlayerState` (per-player) + `GameState` aggregate holding parallel per-position arrays (owner/houses/mortgaged) and a `BankState`. Full `Account`/`Asset`/ledger transfer semantics are deferred to M6 (engine), where effects/ledger matter; M2.5 provides plain data + queries.

**Tech Stack:** C++17, GoogleTest.

---

## File structure

- Create: `src/domain/player.h` — `PlayerState`.
- Create: `src/domain/game_state.h` / `game_state.cpp` — `GameState`, `BankState`.
- Create: `tests/unit/test_game_state.cpp`.
- Modify: `CMakeLists.txt`.

---

### Task 1: PlayerState + GameState data

**Files:** Create `src/domain/player.h`, `src/domain/game_state.h`.

- [ ] **Step 1: `src/domain/player.h`**

```cpp
#pragma once
#include <string>

namespace monopoly::domain {

struct PlayerState {
  std::string name;
  int position = 0;     // current board square (0..39)
  long cash = 0;
  bool inJail = false;
  int jailAttempts = 0;
};

}  // namespace monopoly::domain
```

- [ ] **Step 2: `src/domain/game_state.h`**

```cpp
#pragma once
#include <array>
#include <vector>
#include "domain/board.h"
#include "domain/player.h"

namespace monopoly::domain {

inline constexpr int kHouseSupply = 32;
inline constexpr int kHotelSupply = 12;
inline constexpr int kUnowned = -1;
inline constexpr int kHotel = 5;  // houses value meaning "hotel"

struct BankState {
  int housesAvailable = kHouseSupply;
  int hotelsAvailable = kHotelSupply;
};

// Mutable game ground truth. Holds a reference to the immutable Board.
class GameState {
 public:
  explicit GameState(const Board& board) : board_(board) {
    owner_.fill(kUnowned);
    houses_.fill(0);
    mortgaged_.fill(false);
  }

  const Board& board() const { return board_; }

  // Players ---------------------------------------------------------------
  int addPlayer(const std::string& name, long startingCash);
  int numPlayers() const { return static_cast<int>(players_.size()); }
  PlayerState& player(int id) { return players_[id]; }
  const PlayerState& player(int id) const { return players_[id]; }

  // Ownership / development ----------------------------------------------
  int ownerOf(int pos) const { return owner_[pos]; }
  void setOwner(int pos, int playerId) { owner_[pos] = playerId; }
  int housesOn(int pos) const { return houses_[pos]; }
  void setHouses(int pos, int n) { houses_[pos] = n; }
  bool isMortgaged(int pos) const { return mortgaged_[pos]; }
  void setMortgaged(int pos, bool m) { mortgaged_[pos] = m; }

  // Queries used by the risk layer ---------------------------------------
  bool ownsWholeGroup(int playerId, ColorGroup g) const;
  int countOwnedInGroup(int playerId, ColorGroup g) const;  // STATION/UTILITY use this
  // Player standing on `pos` (kUnowned if none); excludes nobody by default.
  int occupantAt(int pos, int excludePlayer = -999) const;

  BankState& bank() { return bank_; }
  const BankState& bank() const { return bank_; }

  // Free-Parking house pot (count of houses parked there).
  int freeParkingPot() const { return freeParkingPot_; }
  void setFreeParkingPot(int n) { freeParkingPot_ = n; }

 private:
  const Board& board_;
  std::vector<PlayerState> players_;
  std::array<int, kBoardSize> owner_;
  std::array<int, kBoardSize> houses_;
  std::array<bool, kBoardSize> mortgaged_;
  BankState bank_;
  int freeParkingPot_ = 0;
};

}  // namespace monopoly::domain
```

- [ ] **Step 3:** No build yet (impl in Task 2). Proceed.

---

### Task 2: GameState methods + tests

**Files:** Create `src/domain/game_state.cpp`, `tests/unit/test_game_state.cpp`; modify `CMakeLists.txt`.

- [ ] **Step 1: `src/domain/game_state.cpp`**

```cpp
#include "domain/game_state.h"

namespace monopoly::domain {

int GameState::addPlayer(const std::string& name, long startingCash) {
  PlayerState p;
  p.name = name;
  p.cash = startingCash;
  players_.push_back(p);
  return static_cast<int>(players_.size()) - 1;
}

bool GameState::ownsWholeGroup(int playerId, ColorGroup g) const {
  auto members = board_.positionsInGroup(g);
  if (members.empty()) return false;
  for (int pos : members)
    if (owner_[pos] != playerId) return false;
  return true;
}

int GameState::countOwnedInGroup(int playerId, ColorGroup g) const {
  int n = 0;
  for (int pos : board_.positionsInGroup(g))
    if (owner_[pos] == playerId) ++n;
  return n;
}

int GameState::occupantAt(int pos, int excludePlayer) const {
  for (int id = 0; id < numPlayers(); ++id) {
    if (id == excludePlayer) continue;
    if (players_[id].position == pos && !players_[id].inJail) return id;
  }
  return kUnowned;
}

}  // namespace monopoly::domain
```

- [ ] **Step 2: `tests/unit/test_game_state.cpp`**

```cpp
#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/game_state.h"

using namespace monopoly::domain;

TEST(GameState, PlayersAndOwnership) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p0 = gs.addPlayer("P1", 15000000);
  int p1 = gs.addPlayer("P2", 15000000);
  EXPECT_EQ(gs.numPlayers(), 2);
  EXPECT_EQ(gs.ownerOf(39), kUnowned);
  gs.setOwner(39, p0);
  EXPECT_EQ(gs.ownerOf(39), p0);
  EXPECT_EQ(gs.player(p1).cash, 15000000);
}

TEST(GameState, MonopolyAndGroupCounts) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p0 = gs.addPlayer("P1", 0);
  // Brown group = {1,3}.
  EXPECT_FALSE(gs.ownsWholeGroup(p0, ColorGroup::Brown));
  gs.setOwner(1, p0);
  gs.setOwner(3, p0);
  EXPECT_TRUE(gs.ownsWholeGroup(p0, ColorGroup::Brown));
  // Stations = {5,15,25,35}.
  gs.setOwner(5, p0); gs.setOwner(25, p0);
  EXPECT_EQ(gs.countOwnedInGroup(p0, ColorGroup::Station), 2);
}

TEST(GameState, Occupancy) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int p0 = gs.addPlayer("P1", 0);
  int p1 = gs.addPlayer("P2", 0);
  gs.player(p1).position = 24;
  EXPECT_EQ(gs.occupantAt(24), p1);
  EXPECT_EQ(gs.occupantAt(24, /*exclude=*/p1), kUnowned);
  EXPECT_EQ(gs.occupantAt(10), kUnowned);
  (void)p0;
}
```

Add both new source/test files to CMake.

- [ ] **Step 3:** Build + `ctest -R GameState` → PASS. **Commit** `feat(domain): GameState live-state aggregate + queries`.

---

## Milestone 2.5 acceptance
- `ctest` green; ownership/monopoly/group-count/occupancy queries verified.
- All files < 400 LoC.
