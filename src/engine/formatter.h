#pragma once
#include <string>
#include "domain/game_state.h"
#include "engine/effect.h"
#include "engine/style.h"

namespace monopoly::engine {

// Echoed command line as a framed header (red on error).
std::string formatEcho(const std::string& line, bool ok, const Palette& pal);

// Ordered causal side-effect log with per-kind glyphs.
std::string formatEffects(const CommandResult& result, const Palette& pal);

// Expected follow-up actions (suggested commands) the operator should record next.
std::string formatPrompts(const CommandResult& result, const Palette& pal);

// Always-on player panel: positions, cash, jail flag, holdings, pot, bank supply.
// `actingPlayer` (or -1) is marked with '*'.
std::string formatStatePanel(const domain::GameState& gs, int actingPlayer,
                             const Palette& pal);

// Full 40-square board: owners, development, and which players stand where.
std::string formatBoard(const domain::GameState& gs, const Palette& pal);

}  // namespace monopoly::engine
