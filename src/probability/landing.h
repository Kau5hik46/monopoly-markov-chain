#pragma once
#include <vector>
#include "domain/board.h"
#include "domain/deck_factory.h"

namespace monopoly::probability {

// (position, probability); position == kJailSentinel (-1) means "go to jail".
struct LandingProb { int position; double prob; };

// Resolves a raw landing square into terminal destinations, applying
// GO_TO_JAIL redirection and Chance/Community-Chest card draws (with chained
// re-draws, e.g. Back-3 onto a Community Chest square).
class LandingResolver {
 public:
  LandingResolver(const domain::Board& board, const domain::Decks& decks)
      : board_(board), decks_(decks) {}

  std::vector<LandingProb> resolve(int rawPosition) const;

 private:
  void accumulate(int pos, double prob, int depth,
                  std::vector<double>& posMass, double& jailMass) const;
  const domain::Board& board_;
  const domain::Decks& decks_;
};

}  // namespace monopoly::probability
