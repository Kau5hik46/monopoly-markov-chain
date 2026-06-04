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

}  // namespace monopoly::rules
