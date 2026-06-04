#include "risk/rent_table.h"

#include <cstddef>
#include "domain/square.h"
#include "risk/rent_model.h"

namespace monopoly::risk {

using domain::ColorGroup;
using domain::SquareType;

long rentOwed(const domain::GameState& gs, int pos, int mover, int arrivalSum) {
  const auto& sq = gs.board().at(pos);
  if (!domain::isPurchasable(sq.type)) return 0;
  const int owner = gs.ownerOf(pos);
  if (owner == domain::kUnowned || owner == mover) return 0;
  if (gs.isMortgaged(pos)) return 0;

  switch (sq.type) {
    case SquareType::Street: {
      const int h = gs.housesOn(pos);
      if (h == 0) {
        const long site = sq.rent[0];
        return gs.ownsWholeGroup(owner, sq.group)
                   ? site * kMonopolyUndevelopedMultiplier
                   : site;
      }
      const std::size_t idx = (h > 5) ? 5 : static_cast<std::size_t>(h);
      return sq.rent[idx];
    }
    case SquareType::Station:
      return stationRent(gs.countOwnedInGroup(owner, ColorGroup::Station));
    case SquareType::Utility:
      return utilityRent(gs.countOwnedInGroup(owner, ColorGroup::Utility), arrivalSum);
    default:
      return 0;
  }
}

}  // namespace monopoly::risk
