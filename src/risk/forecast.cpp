#include "risk/forecast.h"

#include <random>
#include "probability/micro_state.h"
#include "risk/rent_table.h"

namespace monopoly::risk {

namespace {
// Samples one resolved landing (card-aware) from raw position; -1 means jail.
int sampleLanding(const probability::LandingResolver& resolver, int raw,
                  std::mt19937& rng) {
  auto dist = resolver.resolve(raw);
  std::uniform_real_distribution<double> u(0.0, 1.0);
  double x = u(rng), acc = 0.0;
  for (const auto& lp : dist) {
    acc += lp.prob;
    if (x <= acc) return lp.position;
  }
  return dist.empty() ? raw % probability::kNumPositions : dist.back().position;
}
}  // namespace

NRollForecast forecast(const domain::GameState& gs, int mover, int nRolls,
                       const probability::LandingResolver& resolver, long trials,
                       unsigned seed) {
  NRollForecast f;
  f.horizon = nRolls;
  if (mover < 0 || mover >= gs.numPlayers() || nRolls <= 0 || trials <= 0) return f;

  const long cash = gs.player(mover).cash;
  const int startPos = gs.player(mover).position;
  const bool startJail = gs.player(mover).inJail;
  const int startAtt = gs.player(mover).jailAttempts;

  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> die(1, 6);
  double sumCum = 0.0;
  long ruin = 0;

  for (long t = 0; t < trials; ++t) {
    int pos = startPos, doubles = 0, attempts = startAtt;
    bool inJail = startJail;
    double cum = 0.0;
    bool ruined = false;

    for (int roll = 0; roll < nRolls; ++roll) {
      const int d1 = die(rng), d2 = die(rng), sum = d1 + d2;
      const bool dbl = (d1 == d2);
      int landing;  // -1 = jail
      if (inJail) {
        if (dbl) {
          inJail = false; attempts = 0;
          landing = sampleLanding(resolver, (10 + sum) % probability::kNumPositions, rng);
        } else if (++attempts >= 3) {
          inJail = false; attempts = 0;
          landing = sampleLanding(resolver, (10 + sum) % probability::kNumPositions, rng);
        } else {
          landing = probability::kJailSentinel;  // still in jail
        }
      } else if (dbl && ++doubles >= 3) {
        doubles = 0;
        landing = probability::kJailSentinel;  // three doubles -> jail
      } else {
        if (!dbl) doubles = 0;
        landing = sampleLanding(resolver, (pos + sum) % probability::kNumPositions, rng);
      }

      if (landing == probability::kJailSentinel) {
        inJail = true; pos = 10;
      } else {
        pos = landing;
        cum += static_cast<double>(risk::rentOwed(gs, pos, mover, sum));
      }
      if (!ruined && cash > 0 && cum > static_cast<double>(cash)) ruined = true;
    }
    sumCum += cum;
    if (ruined) ++ruin;
  }

  f.expectedCumulativeRent = sumCum / static_cast<double>(trials);
  f.ruinProbability = static_cast<double>(ruin) / static_cast<double>(trials);
  return f;
}

}  // namespace monopoly::risk
