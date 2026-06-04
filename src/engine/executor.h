#pragma once
#include <vector>
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "engine/command.h"
#include "engine/effect.h"
#include "engine/query_service.h"
#include "rules/rule_config.h"

namespace monopoly::engine {

inline constexpr long kStartingCash = 15000000;
inline constexpr long kFreeParkingHouseCash = 1000000;  // cash per unplaceable house

// Applies parsed commands to the GameState, emitting a causal Effect log and
// integrating the house rules. Supports undo via GameState snapshots (Memento).
class Executor {
 public:
  Executor(domain::GameState& gs, const domain::Decks& decks,
           rules::RuleConfig& rules);

  CommandResult execute(const Command& c);
  int lastMover() const { return lastMover_; }
  const QueryService& queries() const { return query_; }

 private:
  void snapshot();
  bool validPlayer(int id, CommandResult& r) const;
  void resolveLanding(int player, int pos, int arrivalSum, CommandResult& r);

  domain::GameState& gs_;
  rules::RuleConfig& rules_;
  QueryService query_;
  std::vector<domain::GameState> history_;
  int lastMover_ = -1;
};

std::string helpText();

}  // namespace monopoly::engine
