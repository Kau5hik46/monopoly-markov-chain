#pragma once
#include <string>
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "engine/command.h"
#include "engine/effect.h"
#include "engine/style.h"
#include "probability/landing.h"
#include "probability/probability_engine.h"
#include "rules/rule_config.h"

namespace monopoly::engine {

// Answers `query` commands and produces the per-turn advisory, using the analytic
// probability engine and the insurance pricer over the live GameState.
class QueryService {
 public:
  QueryService(const domain::GameState& gs, const domain::Decks& decks,
               const rules::RuleConfig& rules, const Palette& pal);

  CommandResult handle(const Command& c) const;
  // Risk profile + single-roll fair premium for `mover` (shown after every command).
  std::string advisory(int mover) const;

 private:
  const domain::GameState& gs_;
  const rules::RuleConfig& rules_;
  Palette pal_;
  probability::ProbabilityEngine engine_;
  probability::LandingResolver resolver_;
};

}  // namespace monopoly::engine
