#include "probability/monte_carlo.h"

#include "domain/card.h"
#include "domain/square.h"
#include "probability/micro_state.h"

namespace monopoly::probability {

using domain::CardDeck;
using domain::CardMove;
using domain::SquareType;

namespace {
int rollDie(std::mt19937& rng) {
  std::uniform_int_distribution<int> d(1, 6);
  return d(rng);
}
const domain::CardEffect& drawCard(const CardDeck& deck, std::mt19937& rng) {
  std::uniform_int_distribution<int> d(0, deck.size() - 1);
  int r = d(rng);
  for (const auto& e : deck.effects) {
    if (r < e.weight) return e;
    r -= e.weight;
  }
  return deck.effects.back();
}
}  // namespace

MonteCarloEngine::MonteCarloEngine(const domain::Board& board,
                                   const domain::Decks& decks, unsigned seed,
                                   long walkSteps, long trials)
    : board_(board), decks_(decks), seed_(seed), walkSteps_(walkSteps),
      trials_(trials) {}

int MonteCarloEngine::land(int rawPos, Walk& w, std::mt19937& rng) const {
  int pos = rawPos % kNumPositions;
  for (int depth = 0; depth < 8; ++depth) {
    const auto& sq = board_.at(pos);
    if (sq.type == SquareType::GoToJail) {
      w.inJail = true; w.doubles = 0; w.pos = 10;
      return kJailSentinel;
    }
    const CardDeck* deck = nullptr;
    if (sq.type == SquareType::Chance) deck = &decks_.chance;
    else if (sq.type == SquareType::CommunityChest) deck = &decks_.communityChest;
    if (deck == nullptr) break;  // terminal square

    const auto& e = drawCard(*deck, rng);
    if (e.move == CardMove::None) break;  // stay
    if (e.move == CardMove::GoToJail) { w.inJail = true; w.doubles = 0; w.pos = 10; return kJailSentinel; }
    if (e.move == CardMove::AdvanceTo) pos = e.target;
    else if (e.move == CardMove::NearestStation) pos = board_.nearestForward(pos, SquareType::Station);
    else if (e.move == CardMove::NearestUtility) pos = board_.nearestForward(pos, SquareType::Utility);
    else if (e.move == CardMove::Back3) pos = (pos - 3 + kNumPositions) % kNumPositions;
  }
  w.pos = pos;
  return pos;
}

int MonteCarloEngine::step(Walk& w, std::mt19937& rng) const {
  const int d1 = rollDie(rng), d2 = rollDie(rng);
  const int sum = d1 + d2;
  const bool dbl = (d1 == d2);
  if (w.inJail) {
    if (dbl) { w.inJail = false; w.attempts = 0; return land((10 + sum), w, rng); }
    ++w.attempts;
    if (w.attempts >= 3) { w.inJail = false; w.attempts = 0; return land((10 + sum), w, rng); }
    return kJailSentinel;  // stay in jail
  }
  if (dbl) {
    if (++w.doubles >= 3) { w.doubles = 0; w.inJail = true; w.pos = 10; return kJailSentinel; }
  } else {
    w.doubles = 0;
  }
  return land(w.pos + sum, w, rng);
}

PositionVector MonteCarloEngine::stationaryByPosition() const {
  std::mt19937 rng(seed_);
  Walk w;
  PositionVector counts{};
  counts.fill(0.0);
  for (long i = 0; i < walkSteps_; ++i) {
    int p = step(w, rng);
    if (p != kJailSentinel) counts[static_cast<std::size_t>(p)] += 1.0;
  }
  for (double& x : counts) x /= static_cast<double>(walkSteps_);
  return counts;
}

double MonteCarloEngine::jailProbability() const {
  std::mt19937 rng(seed_);
  Walk w;
  long jail = 0;
  for (long i = 0; i < walkSteps_; ++i)
    if (step(w, rng) == kJailSentinel) ++jail;
  return static_cast<double>(jail) / static_cast<double>(walkSteps_);
}

PositionVector MonteCarloEngine::singleRoll(int fromPos) const {
  std::mt19937 rng(seed_);
  PositionVector counts{};
  counts.fill(0.0);
  for (long t = 0; t < trials_; ++t) {
    Walk w; w.pos = fromPos % kNumPositions;
    int p = step(w, rng);
    if (p != kJailSentinel) counts[static_cast<std::size_t>(p)] += 1.0;
  }
  for (double& x : counts) x /= static_cast<double>(trials_);
  return counts;
}

PositionVector MonteCarloEngine::afterNRolls(int fromPos, int n) const {
  std::mt19937 rng(seed_);
  PositionVector counts{};
  counts.fill(0.0);
  for (long t = 0; t < trials_; ++t) {
    Walk w; w.pos = fromPos % kNumPositions;
    int last = w.pos;
    bool jail = false;
    for (int i = 0; i < n; ++i) { int p = step(w, rng); jail = (p == kJailSentinel); last = jail ? -1 : p; }
    if (last >= 0) counts[static_cast<std::size_t>(last)] += 1.0;
  }
  for (double& x : counts) x /= static_cast<double>(trials_);
  return counts;
}

}  // namespace monopoly::probability
