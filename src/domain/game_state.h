#pragma once
#include <array>
#include <string>
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
  explicit GameState(const Board& board) : board_(&board) {
    owner_.fill(kUnowned);
    houses_.fill(0);
    mortgaged_.fill(false);
  }
  // Copyable/assignable (board held by pointer) so the executor can snapshot for undo.

  const Board& board() const { return *board_; }

  // Players ---------------------------------------------------------------
  int addPlayer(const std::string& name, long startingCash);
  int numPlayers() const { return static_cast<int>(players_.size()); }
  PlayerState& player(int id) { return players_[static_cast<std::size_t>(id)]; }
  const PlayerState& player(int id) const {
    return players_[static_cast<std::size_t>(id)];
  }

  // Ownership / development ----------------------------------------------
  int ownerOf(int pos) const { return owner_[static_cast<std::size_t>(pos)]; }
  void setOwner(int pos, int playerId) {
    owner_[static_cast<std::size_t>(pos)] = playerId;
  }
  int housesOn(int pos) const { return houses_[static_cast<std::size_t>(pos)]; }
  void setHouses(int pos, int n) { houses_[static_cast<std::size_t>(pos)] = n; }
  bool isMortgaged(int pos) const {
    return mortgaged_[static_cast<std::size_t>(pos)];
  }
  void setMortgaged(int pos, bool m) {
    mortgaged_[static_cast<std::size_t>(pos)] = m;
  }

  // Queries used by the risk layer ---------------------------------------
  bool ownsWholeGroup(int playerId, ColorGroup g) const;
  int countOwnedInGroup(int playerId, ColorGroup g) const;  // STATION/UTILITY use this
  // Player standing on `pos` (kUnowned if none), optionally excluding one player.
  int occupantAt(int pos, int excludePlayer = -999) const;

  BankState& bank() { return bank_; }
  const BankState& bank() const { return bank_; }

  // Free-Parking house pot (count of houses parked there).
  int freeParkingPot() const { return freeParkingPot_; }
  void setFreeParkingPot(int n) { freeParkingPot_ = n; }

 private:
  const Board* board_;
  std::vector<PlayerState> players_;
  std::array<int, kBoardSize> owner_;
  std::array<int, kBoardSize> houses_;
  std::array<bool, kBoardSize> mortgaged_;
  BankState bank_;
  int freeParkingPot_ = 0;
};

}  // namespace monopoly::domain
