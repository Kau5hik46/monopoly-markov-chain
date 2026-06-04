#include "rules/rule_logic.h"

namespace monopoly::rules {

int freeParkingHousesForTax(domain::SquareType taxType, const RuleConfig& cfg) {
  if (!cfg.freeParkingPotEnabled) return 0;
  switch (taxType) {
    case domain::SquareType::IncomeTax: return cfg.housesPerIncomeTax;
    case domain::SquareType::SuperTax: return cfg.housesPerSuperTax;
    default: return 0;
  }
}

bool canTravelBetweenAirports(const domain::GameState& gs, int player, int from,
                              int to) {
  if (from == to) return false;
  const auto& a = gs.board().at(from);
  const auto& b = gs.board().at(to);
  if (a.type != domain::SquareType::Station || b.type != domain::SquareType::Station)
    return false;
  return gs.ownerOf(from) == player && gs.ownerOf(to) == player;
}

namespace {
// A street is buildable by `player` if its whole group is owned, none mortgaged,
// and it has room (< hotel).
bool streetBuildable(const domain::GameState& gs, int player, int pos) {
  const auto& sq = gs.board().at(pos);
  if (sq.type != domain::SquareType::Street) return false;
  if (gs.housesOn(pos) >= domain::kHotel) return false;
  if (!gs.ownsWholeGroup(player, sq.group)) return false;
  for (int m : gs.board().positionsInGroup(sq.group))
    if (gs.isMortgaged(m)) return false;
  return true;
}
}  // namespace

ClaimResult claimFreeParkingPot(domain::GameState& gs, int player) {
  ClaimResult res;
  int pot = gs.freeParkingPot();
  while (pot > 0) {
    int best = -1, bestHouses = domain::kHotel + 1;  // place on the fewest-house street
    for (int pos = 0; pos < domain::kBoardSize; ++pos) {
      if (!streetBuildable(gs, player, pos)) continue;
      if (gs.housesOn(pos) < bestHouses) { best = pos; bestHouses = gs.housesOn(pos); }
    }
    if (best < 0) break;  // nothing legal to build on
    gs.setHouses(best, gs.housesOn(best) + 1);
    ++res.placed;
    --pot;
  }
  res.cashed = pot;
  gs.bank().housesAvailable += res.cashed;  // unplaced houses go back to the bank
  gs.setFreeParkingPot(0);
  return res;
}

}  // namespace monopoly::rules
