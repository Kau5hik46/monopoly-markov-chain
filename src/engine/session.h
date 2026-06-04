#pragma once
#include <string>
#include "domain/game_state.h"
#include "rules/rule_config.h"

namespace monopoly::engine {

// Serializes the full game state + rules + whose-turn to a JSON file.
void saveGame(const domain::GameState& gs, const rules::RuleConfig& rules,
              int nextRoller, const std::string& path);

// Restores state/rules/turn from a JSON file, rebuilding `gs` in place.
// Throws std::runtime_error on a missing/malformed file.
void loadGame(domain::GameState& gs, rules::RuleConfig& rules, int& nextRoller,
              const std::string& path);

}  // namespace monopoly::engine
