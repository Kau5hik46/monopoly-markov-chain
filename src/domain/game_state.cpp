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
  auto members = board_->positionsInGroup(g);
  if (members.empty()) return false;
  for (int pos : members)
    if (owner_[static_cast<std::size_t>(pos)] != playerId) return false;
  return true;
}

int GameState::countOwnedInGroup(int playerId, ColorGroup g) const {
  int n = 0;
  for (int pos : board_->positionsInGroup(g))
    if (owner_[static_cast<std::size_t>(pos)] == playerId) ++n;
  return n;
}

int GameState::occupantAt(int pos, int excludePlayer) const {
  for (int id = 0; id < numPlayers(); ++id) {
    if (id == excludePlayer) continue;
    const auto& p = players_[static_cast<std::size_t>(id)];
    if (p.position == pos && !p.inJail) return id;
  }
  return kUnowned;
}

}  // namespace monopoly::domain
