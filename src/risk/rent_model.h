#pragma once

namespace monopoly::risk {

inline constexpr long kMonopolyUndevelopedMultiplier = 2;  // 2x site for full set

// Station rent by number owned: £25/50/100/200 x10000.
inline long stationRent(int countOwned) {
  switch (countOwned) {
    case 1: return 250000;
    case 2: return 500000;
    case 3: return 1000000;
    case 4: return 2000000;
    default: return 0;
  }
}

// Utility rent: 4x dice (one owned) or 10x dice (both), x10000 board scale.
inline long utilityRent(int countOwned, int arrivalSum) {
  const long perPip = (countOwned >= 2) ? 100000 : 40000;
  return perPip * static_cast<long>(arrivalSum);
}

}  // namespace monopoly::risk
