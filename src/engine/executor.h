#pragma once
#include <vector>
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "engine/command.h"
#include "engine/effect.h"
#include "engine/query_service.h"
#include "engine/style.h"
#include "rules/rule_config.h"

namespace monopoly::engine {

inline constexpr long kStartingCash = 15000000;
inline constexpr long kFreeParkingHouseCash = 1000000;  // cash per unplaceable house

// Applies parsed commands to the GameState, emitting a causal Effect log and
// integrating the house rules. Supports undo via GameState snapshots (Memento).
class Executor {
 public:
  Executor(domain::GameState& gs, const domain::Decks& decks,
           rules::RuleConfig& rules, const Palette& pal = Palette{});

  CommandResult execute(const Command& c);
  int lastMover() const { return lastMover_; }       // last player to act (for '*')
  int nextRoller() const { return nextRoller_; }      // whose turn to roll next
  const QueryService& queries() const { return query_; }

 private:
  void snapshot();
  bool validPlayer(int id, CommandResult& r) const;
  void resolveLanding(int player, int pos, int arrivalSum, CommandResult& r);

  domain::GameState& gs_;
  rules::RuleConfig& rules_;
  Palette pal_;
  QueryService query_;
  std::vector<domain::GameState> history_;
  int lastMover_ = -1;
  int nextRoller_ = 0;
};

}  // namespace monopoly::engine
