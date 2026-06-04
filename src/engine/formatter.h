#pragma once
#include <string>
#include "domain/game_state.h"
#include "engine/effect.h"

namespace monopoly::engine {

// Renders the ordered causal side-effect log of a command.
std::string formatEffects(const CommandResult& result);

// Renders the always-on game-state panel: each player's position/cash/jail, plus
// a compact ownership summary and the Free-Parking pot / bank supply.
std::string formatStatePanel(const domain::GameState& gs);

}  // namespace monopoly::engine
