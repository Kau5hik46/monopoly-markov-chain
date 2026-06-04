#pragma once
#include <array>
#include "probability/micro_state.h"

namespace monopoly::probability {

using PositionVector = std::array<double, kNumPositions>;

// Common interface so analytic and (later) Monte-Carlo backends are interchangeable.
class IDistributionProvider {
 public:
  virtual ~IDistributionProvider() = default;
  // Long-run landing probability per board position (jail mass reported separately).
  virtual PositionVector stationaryByPosition() const = 0;
  virtual double jailProbability() const = 0;
  // Distribution over landing squares after exactly one throw from `fromPos` (d=0).
  virtual PositionVector singleRoll(int fromPos) const = 0;
  // Distribution after `n` throws from `fromPos`.
  virtual PositionVector afterNRolls(int fromPos, int n) const = 0;
};

}  // namespace monopoly::probability
