#include "risk/rent_table.h"

#include <cstddef>
#include "domain/board.h"
#include "domain/square.h"

namespace monopoly::risk {

using domain::ColorGroup;
using domain::SquareType;

long rentOwed(const domain::GameState& gs, int pos, int mover, int arrivalSum) {
  const auto& sq = gs.board().at(pos);
  if (!domain::isPurchasable(sq.type)) return 0;
  const int owner = gs.ownerOf(pos);
  if (owner == domain::kUnowned || owner == mover) return 0;
  if (gs.isMortgaged(pos)) return 0;

  const domain::RentRules& rules = gs.board().rentRules();

  switch (sq.type) {
    case SquareType::Street: {
      const int h = gs.housesOn(pos);
      if (h == 0) {
        const long site = sq.rent[0];
        return gs.ownsWholeGroup(owner, sq.group)
                   ? site * rules.monopolyUndevelopedMultiplier
                   : site;
      }
      const std::size_t idx = (h > 5) ? 5 : static_cast<std::size_t>(h);
      return sq.rent[idx];
    }
    case SquareType::Station: {
      const int count = gs.countOwnedInGroup(owner, ColorGroup::Station);
      if (count < 1 || count > 4) return 0;
      return rules.stationRentByCount[static_cast<std::size_t>(count - 1)];
    }
    case SquareType::Utility: {
      const int count = gs.countOwnedInGroup(owner, ColorGroup::Utility);
      const long perPip = (count >= 2) ? rules.utilityPerPipBoth : rules.utilityPerPipOne;
      return perPip * static_cast<long>(arrivalSum);
    }
    default:
      return 0;
  }
}

}  // namespace monopoly::risk
