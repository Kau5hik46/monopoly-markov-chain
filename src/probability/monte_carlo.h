#pragma once
#include <random>
#include "domain/board.h"
#include "domain/deck_factory.h"
#include "probability/distribution_provider.h"

namespace monopoly::probability {

// Seeded position-only Monte-Carlo simulator. Mirrors the analytic chain (dice,
// doubles->jail, jail policy, stochastic card draws) so it can validate it.
// Implements the same IDistributionProvider interface (Bridge).
class MonteCarloEngine : public IDistributionProvider {
 public:
  MonteCarloEngine(const domain::Board& board, const domain::Decks& decks,
                   unsigned seed = 12345u, long walkSteps = 2000000,
                   long trials = 200000);

  PositionVector stationaryByPosition() const override;
  double jailProbability() const override;
  PositionVector singleRoll(int fromPos) const override;
  PositionVector afterNRolls(int fromPos, int n) const override;

 private:
  // Mutable walker state.
  struct Walk { int pos = 0; int doubles = 0; bool inJail = false; int attempts = 0; };
  // Advances one die-throw; returns landing position, or kJailSentinel for jail.
  int step(Walk& w, std::mt19937& rng) const;
  // Resolves a raw square stochastically (GO_TO_JAIL + one card draw); updates w.pos.
  int land(int rawPos, Walk& w, std::mt19937& rng) const;

  const domain::Board& board_;
  const domain::Decks& decks_;
  unsigned seed_;
  long walkSteps_;
  long trials_;
};

}  // namespace monopoly::probability
