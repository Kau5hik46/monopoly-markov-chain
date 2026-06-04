#pragma once
#include "domain/game_state.h"
#include "probability/landing.h"

namespace monopoly::risk {

struct NRollForecast {
  int horizon = 0;                     // number of rolls simulated
  double expectedCumulativeRent = 0.0; // req B: E[total rent over N rolls]
  double ruinProbability = 0.0;        // req D: P(cumulative rent > cash within N)
};

// Monte-Carlo forecast of cumulative rent and ruin for `mover` over `nRolls`,
// simulating their movement (dice + doubles/jail + card-aware sampled landings) with
// ownership held fixed. Seeded for reproducibility.
NRollForecast forecast(const domain::GameState& gs, int mover, int nRolls,
                       const probability::LandingResolver& resolver,
                       long trials = 50000, unsigned seed = 1234u);

}  // namespace monopoly::risk
