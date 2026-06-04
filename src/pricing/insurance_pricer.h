#pragma once
#include "domain/game_state.h"
#include "probability/landing.h"

namespace monopoly::pricing {

struct PricingConfig {
  bool muggingEnabled = false;
  long muggingAmount = 500000;  // £ transferred when the mugger wins
};

struct RollQuote {
  double expectedRent = 0.0;      // fair premium to insure next-roll rent liability
  double muggingExposure = 0.0;   // signed EV from mugging (positive = expected gain)
  double fairPremium = 0.0;       // expectedRent net of mugging benefit (>=0, floored)
  double maxRent = 0.0;           // worst-case single-roll rent over reachable squares
  int maxRentSquare = -1;         // which square produces maxRent (-1 if none)
};

// Prices the next single roll for `mover` from their current position, using the
// known positions/ownership in `gs`. Exact for one roll (positions are known).
RollQuote priceNextRoll(const domain::GameState& gs, int mover,
                        const probability::LandingResolver& resolver,
                        const PricingConfig& cfg);

}  // namespace monopoly::pricing
