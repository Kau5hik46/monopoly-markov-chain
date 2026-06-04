#include "probability/probability_engine.h"

#include "math/matrix.h"
#include "probability/micro_state.h"

namespace monopoly::probability {

ProbabilityEngine::ProbabilityEngine(const domain::Board& board,
                                     const domain::Decks& decks)
    : tm_(board, decks) {}

std::vector<double> ProbabilityEngine::stepFrom(
    const std::vector<double>& state) const {
  return tm_.matrix().vecMul(state);
}

PositionVector ProbabilityEngine::marginalize(const std::vector<double>& state,
                                              double* jailOut) {
  PositionVector pos{};
  pos.fill(0.0);
  double jail = 0.0;
  for (int i = 0; i < kNumStates; ++i) {
    if (isJailState(i)) jail += state[i];
    else pos[positionOf(i)] += state[i];
  }
  if (jailOut) *jailOut = jail;
  return pos;
}

PositionVector ProbabilityEngine::stationaryByPosition() const {
  auto pi = math::stationaryDistribution(tm_.matrix());
  double jail = 0.0;
  return marginalize(pi, &jail);
}

double ProbabilityEngine::jailProbability() const {
  auto pi = math::stationaryDistribution(tm_.matrix());
  double jail = 0.0;
  marginalize(pi, &jail);
  return jail;
}

PositionVector ProbabilityEngine::singleRoll(int fromPos) const {
  std::vector<double> state(kNumStates, 0.0);
  state[onBoardIndex(fromPos % kNumPositions, 0)] = 1.0;
  auto next = stepFrom(state);
  double jail = 0.0;
  return marginalize(next, &jail);
}

PositionVector ProbabilityEngine::afterNRolls(int fromPos, int n) const {
  std::vector<double> state(kNumStates, 0.0);
  state[onBoardIndex(fromPos % kNumPositions, 0)] = 1.0;
  for (int i = 0; i < n; ++i) state = stepFrom(state);
  double jail = 0.0;
  return marginalize(state, &jail);
}

}  // namespace monopoly::probability
