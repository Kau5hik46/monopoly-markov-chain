#include "probability/landing.h"

#include "domain/card.h"
#include "probability/micro_state.h"

namespace monopoly::probability {

using domain::CardMove;
using domain::SquareType;

void LandingResolver::accumulate(int pos, double prob, int depth,
                                 std::vector<double>& posMass,
                                 double& jailMass) const {
  if (depth > 8) {  // safety guard; real chains are shallow
    posMass[pos] += prob;
    return;
  }
  const auto& sq = board_.at(pos);
  if (sq.type == SquareType::GoToJail) {
    jailMass += prob;
    return;
  }
  const domain::CardDeck* deck = nullptr;
  if (sq.type == SquareType::Chance) deck = &decks_.chance;
  else if (sq.type == SquareType::CommunityChest) deck = &decks_.communityChest;

  if (deck == nullptr) {
    posMass[pos] += prob;  // terminal square
    return;
  }

  const double n = static_cast<double>(deck->size());
  for (const auto& e : deck->effects) {
    const double p = prob * (static_cast<double>(e.weight) / n);
    switch (e.move) {
      case CardMove::None:
        posMass[pos] += p;  // stay, no redraw
        break;
      case CardMove::GoToJail:
        jailMass += p;
        break;
      case CardMove::AdvanceTo:
        accumulate(e.target, p, depth + 1, posMass, jailMass);
        break;
      case CardMove::NearestStation:
        accumulate(board_.nearestForward(pos, SquareType::Station), p, depth + 1,
                   posMass, jailMass);
        break;
      case CardMove::NearestUtility:
        accumulate(board_.nearestForward(pos, SquareType::Utility), p, depth + 1,
                   posMass, jailMass);
        break;
      case CardMove::Back3:
        accumulate((pos - 3 + kNumPositions) % kNumPositions, p, depth + 1,
                   posMass, jailMass);
        break;
    }
  }
}

std::vector<LandingProb> LandingResolver::resolve(int rawPosition) const {
  std::vector<double> posMass(kNumPositions, 0.0);
  double jailMass = 0.0;
  accumulate(rawPosition % kNumPositions, 1.0, 0, posMass, jailMass);

  std::vector<LandingProb> out;
  for (int p = 0; p < kNumPositions; ++p)
    if (posMass[p] > 0.0) out.push_back({p, posMass[p]});
  if (jailMass > 0.0) out.push_back({kJailSentinel, jailMass});
  return out;
}

}  // namespace monopoly::probability
