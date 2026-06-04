#pragma once
#include <vector>
#include "probability/distribution_provider.h"
#include "probability/transition_matrix.h"
#include "domain/board.h"
#include "domain/deck_factory.h"

namespace monopoly::probability {

// Analytic Markov implementation of IDistributionProvider.
class ProbabilityEngine : public IDistributionProvider {
 public:
  ProbabilityEngine(const domain::Board& board, const domain::Decks& decks);

  PositionVector stationaryByPosition() const override;
  double jailProbability() const override;
  PositionVector singleRoll(int fromPos) const override;
  PositionVector afterNRolls(int fromPos, int n) const override;

 private:
  std::vector<double> stepFrom(const std::vector<double>& state) const;
  static PositionVector marginalize(const std::vector<double>& state,
                                    double* jailOut);
  TransitionMatrix tm_;
};

}  // namespace monopoly::probability
