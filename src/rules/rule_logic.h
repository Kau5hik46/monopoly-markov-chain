#pragma once
#include "domain/game_state.h"
#include "domain/square.h"
#include "rules/rule_config.h"

namespace monopoly::rules {

enum class MuggingResult { MuggerWins, MuggeeEscapes };

// Mugger wins only on a strictly greater contest total; ties favor the muggee.
inline MuggingResult resolveMugging(int muggerSum, int muggeeSum) {
  return muggerSum > muggeeSum ? MuggingResult::MuggerWins
                               : MuggingResult::MuggeeEscapes;
}

// Houses added to the Free-Parking pot when a tax is paid to the bank.
int freeParkingHousesForTax(domain::SquareType taxType, const RuleConfig& cfg);

// Mugging happens everywhere except Jail and Free Parking.
inline bool isMuggingEligible(domain::SquareType t) {
  return t != domain::SquareType::Jail && t != domain::SquareType::FreeParking;
}

// Airport travel is legal only between two distinct airports both owned by `player`.
bool canTravelBetweenAirports(const domain::GameState& gs, int player, int from,
                              int to);

struct ClaimResult {
  int placed = 0;   // houses placed onto the claimer's monopoly streets
  int cashed = 0;   // unplaceable houses, converted to cash & returned to the bank
};

// Claims the free-parking house pot for `player`: places houses (even-building) onto
// streets in fully-owned, unmortgaged groups; leftover houses return to the bank and
// become cash. Mutates `gs` (houses, bank supply, pot). Returns the split.
ClaimResult claimFreeParkingPot(domain::GameState& gs, int player);

}  // namespace monopoly::rules
