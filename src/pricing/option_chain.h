#pragma once
#include <vector>
#include "domain/game_state.h"
#include "probability/landing.h"

namespace monopoly::pricing {

// One reachable next-roll rent outcome: pay `rent` with probability `prob`.
struct LossOutcome {
  double prob;
  double rent;
};

// One strike row of the option chain.
struct OptionRow {
  double strike = 0.0;       // deductible K
  double payoutProb = 0.0;   // P(L > K)
  double fairPremium = 0.0;  // E[max(L - K, 0)]
};

struct OptionChainResult {
  double expectedLoss = 0.0;  // E[L] == premium at strike 0
  double maxLoss = 0.0;       // worst-case single-roll rent
  std::vector<OptionRow> rows;
};

// Discrete distribution of next-roll rent liability for `mover` (rent>0 outcomes).
std::vector<LossOutcome> buildLossDistribution(
    const domain::GameState& gs, int mover,
    const probability::LandingResolver& resolver);

// Option chain over a nicely-rounded strike ladder (up to ~maxRows strikes).
OptionChainResult buildOptionChain(const domain::GameState& gs, int mover,
                                   const probability::LandingResolver& resolver,
                                   int maxRows = 8);

// Call fair value E[max(L - K, 0)] at an arbitrary strike K over a loss distribution.
double fairPremiumAtStrike(const std::vector<LossOutcome>& loss, double K);

// Generalized fair value for a call (isPut=false) or put (isPut=true). The loss
// distribution only carries rent>0 outcomes; the put adds the no-liability mass
// P(L=0)=1-Σprob at value 0, paying K there: put = (1-Σprob)*K + Σ max(K-rent,0).
double fairValueAtStrike(const std::vector<LossOutcome>& loss, double K, bool isPut);

// Worst-case single-roll rent over the distribution (0 if empty).
double maxLossOf(const std::vector<LossOutcome>& loss);

}  // namespace monopoly::pricing
