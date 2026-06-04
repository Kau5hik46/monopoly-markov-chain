#pragma once
#include "math/matrix.h"
#include "probability/landing.h"
#include "domain/board.h"
#include "domain/deck_factory.h"

namespace monopoly::probability {

// Builds the 123x123 row-stochastic per-roll transition matrix.
class TransitionMatrix {
 public:
  TransitionMatrix(const domain::Board& board, const domain::Decks& decks);
  const math::Matrix& matrix() const { return p_; }

 private:
  void addLanding(int fromState, int rawTarget, int nextDoubles, double weight);
  math::Matrix p_;
  LandingResolver resolver_;
};

}  // namespace monopoly::probability
