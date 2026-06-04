#include "pricing/insurance_pricer.h"

#include <algorithm>
#include "domain/square.h"
#include "probability/dice.h"
#include "probability/micro_state.h"
#include "risk/rent_table.h"

namespace monopoly::pricing {

RollQuote priceNextRoll(const domain::GameState& gs, int mover,
                        const probability::LandingResolver& resolver,
                        const PricingConfig& cfg) {
  const int from = gs.player(mover).position;
  const double pMuggerWins = probability::pMuggerWins();

  RollQuote q;
  // Enumerate the 36 equally-likely dice outcomes; arrivalSum is known per outcome,
  // so utility rent is exact.
  for (int d1 = 1; d1 <= 6; ++d1) {
    for (int d2 = 1; d2 <= 6; ++d2) {
      const int sum = d1 + d2;
      const int raw = (from + sum) % probability::kNumPositions;
      for (const auto& lp : resolver.resolve(raw)) {
        if (lp.position == probability::kJailSentinel) continue;  // no rent in jail
        const double w = (1.0 / 36.0) * lp.prob;
        q.expectedRent +=
            w * static_cast<double>(risk::rentOwed(gs, lp.position, mover, sum));
        if (cfg.muggingEnabled) {
          const int occ = gs.occupantAt(lp.position, /*exclude=*/mover);
          const auto& sq = gs.board().at(lp.position);
          const bool eligible = sq.type != domain::SquareType::Jail &&
                                sq.type != domain::SquareType::FreeParking;
          if (occ != domain::kUnowned && eligible) {
            // Mover is the mugger: wins -> +amount; loses -> goes to jail (no cash).
            q.muggingExposure +=
                w * (pMuggerWins * static_cast<double>(cfg.muggingAmount));
          }
        }
      }
    }
  }
  q.fairPremium = std::max(0.0, q.expectedRent - std::max(0.0, q.muggingExposure));
  return q;
}

}  // namespace monopoly::pricing
