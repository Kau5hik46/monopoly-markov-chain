#pragma once
#include "domain/game_state.h"

namespace monopoly::risk {

// Rent a non-owner owes for ending their move on `pos`, given the board state and
// the dice sum that brought them there (only utilities use arrivalSum).
// Returns 0 if unowned, owned by `mover`, mortgaged, or not a property square.
long rentOwed(const domain::GameState& gs, int pos, int mover, int arrivalSum);

}  // namespace monopoly::risk
