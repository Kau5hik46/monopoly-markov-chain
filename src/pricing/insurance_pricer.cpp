#include "pricing/insurance_pricer.h"

#include <algorithm>
#include <array>
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

  std::array<double, probability::kNumPositions> prob{};
  std::array<double, probability::kNumPositions> contrib{};
  prob.fill(0.0);
  contrib.fill(0.0);

  RollQuote q;
  for (int d1 = 1; d1 <= 6; ++d1) {
    for (int d2 = 1; d2 <= 6; ++d2) {
      const int sum = d1 + d2;
      const int raw = (from + sum) % probability::kNumPositions;
      for (const auto& lp : resolver.resolve(raw)) {
        if (lp.position == probability::kJailSentinel) continue;  // no rent in jail
        const double w = (1.0 / 36.0) * lp.prob;
        const long rent = risk::rentOwed(gs, lp.position, mover, sum);
        q.expectedRent += w * static_cast<double>(rent);
        if (rent > 0) {
          prob[static_cast<std::size_t>(lp.position)] += w;
          contrib[static_cast<std::size_t>(lp.position)] += w * static_cast<double>(rent);
        }
        if (static_cast<double>(rent) > q.maxRent) {
          q.maxRent = static_cast<double>(rent);
          q.maxRentSquare = lp.position;
        }
        if (cfg.muggingEnabled) {
          const int occ = gs.occupantAt(lp.position, /*exclude=*/mover);
          const auto& sq = gs.board().at(lp.position);
          const bool eligible = sq.type != domain::SquareType::Jail &&
                                sq.type != domain::SquareType::FreeParking;
          if (occ != domain::kUnowned && eligible)
            q.muggingExposure += w * (pMuggerWins * static_cast<double>(cfg.muggingAmount));
        }
      }
    }
  }

  for (int p = 0; p < probability::kNumPositions; ++p)
    if (contrib[static_cast<std::size_t>(p)] > 0.0)
      q.threats.push_back({p, prob[static_cast<std::size_t>(p)],
                           contrib[static_cast<std::size_t>(p)]});
  std::sort(q.threats.begin(), q.threats.end(),
            [](const Threat& a, const Threat& b) {
              return a.expectedContribution > b.expectedContribution;
            });

  q.fairPremium = std::max(0.0, q.expectedRent - std::max(0.0, q.muggingExposure));
  return q;
}

}  // namespace monopoly::pricing
