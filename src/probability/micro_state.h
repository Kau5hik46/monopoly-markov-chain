#pragma once
#include <cstddef>

namespace monopoly::probability {

inline constexpr int kNumPositions = 40;
inline constexpr int kMaxDoubles = 3;            // d in {0,1,2}
inline constexpr int kOnBoardStates = kNumPositions * kMaxDoubles;  // 120
inline constexpr int kJailStates = 3;            // attempts a in {0,1,2}
inline constexpr int kNumStates = kOnBoardStates + kJailStates;     // 123
inline constexpr int kJailSentinel = -1;         // resolveLanding "go to jail"

inline int onBoardIndex(int pos, int doubles) { return pos * kMaxDoubles + doubles; }
inline int jailIndex(int attempts) { return kOnBoardStates + attempts; }
inline bool isJailState(int idx) { return idx >= kOnBoardStates; }
inline int positionOf(int onBoardIdx) { return onBoardIdx / kMaxDoubles; }
inline int doublesOf(int onBoardIdx) { return onBoardIdx % kMaxDoubles; }

}  // namespace monopoly::probability
