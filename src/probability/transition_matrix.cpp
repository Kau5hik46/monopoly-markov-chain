#include "probability/transition_matrix.h"

#include "probability/micro_state.h"

namespace monopoly::probability {

namespace {
constexpr double kPerOutcome = 1.0 / 36.0;
}

// Distributes `weight` from `fromState` across the resolved landings of moving to
// `rawTarget`; jail sentinel routes to in-jail(0); board squares carry `nextDoubles`.
void TransitionMatrix::addLanding(int fromState, int rawTarget, int nextDoubles,
                                  double weight) {
  for (const auto& lp : resolver_.resolve(rawTarget)) {
    if (lp.position == kJailSentinel)
      p_(fromState, jailIndex(0)) += weight * lp.prob;
    else
      p_(fromState, onBoardIndex(lp.position, nextDoubles)) += weight * lp.prob;
  }
}

TransitionMatrix::TransitionMatrix(const domain::Board& board,
                                   const domain::Decks& decks)
    : p_(kNumStates, kNumStates, 0.0), resolver_(board, decks) {
  // On-board states.
  for (int pos = 0; pos < kNumPositions; ++pos) {
    for (int d = 0; d < kMaxDoubles; ++d) {
      const int from = onBoardIndex(pos, d);
      for (int a = 1; a <= 6; ++a) {
        for (int b = 1; b <= 6; ++b) {
          const bool dbl = (a == b);
          const int sum = a + b;
          if (dbl && d == kMaxDoubles - 1) {
            p_(from, jailIndex(0)) += kPerOutcome;  // 3rd double -> jail
          } else {
            const int nextD = dbl ? d + 1 : 0;
            addLanding(from, (pos + sum) % kNumPositions, nextD, kPerOutcome);
          }
        }
      }
    }
  }
  // In-jail states.
  for (int a = 0; a < kJailStates; ++a) {
    const int from = jailIndex(a);
    for (int d1 = 1; d1 <= 6; ++d1) {
      for (int d2 = 1; d2 <= 6; ++d2) {
        const bool dbl = (d1 == d2);
        const int sum = d1 + d2;
        if (dbl) {
          addLanding(from, (10 + sum) % kNumPositions, 0, kPerOutcome);
        } else if (a < kJailStates - 1) {
          p_(from, jailIndex(a + 1)) += kPerOutcome;  // stay in jail
        } else {
          addLanding(from, (10 + sum) % kNumPositions, 0, kPerOutcome);  // pay & move
        }
      }
    }
  }
}

}  // namespace monopoly::probability
