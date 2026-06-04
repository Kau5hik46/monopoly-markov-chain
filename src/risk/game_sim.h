#pragma once
#include <vector>
#include "domain/game_state.h"
#include "probability/landing.h"
#include "rules/rule_config.h"

namespace monopoly::risk {

struct PlayerOutcome {
  double ruinProbability = 0.0;     // P(cash goes negative within the horizon)
  double expectedCashDelta = 0.0;   // mean(final cash - starting cash)
  double expectedTimesRobbed = 0.0; // mean muggings suffered (sent to hospital)
};

struct GameForecast {
  int rounds = 0;
  std::vector<PlayerOutcome> players;
};

// Multi-player rule-coupled Monte-Carlo: clones the live state and simulates `rounds`
// turns for every player, applying movement (dice/doubles/jail/cards), rent, and the
// coupling rules (mugging displaces muggee->Free Parking / mugger->Jail). Ownership
// and development are held fixed. Seeded for reproducibility.
//
// Simplifications (documented): no tax cash / money cards / buying / airport travel /
// free-parking pot — those need operator input and don't couple movement.
GameForecast simulateGame(const domain::GameState& gs, const rules::RuleConfig& rules,
                          int rounds, const probability::LandingResolver& resolver,
                          long trials = 20000, unsigned seed = 99u);

}  // namespace monopoly::risk
