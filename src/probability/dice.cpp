#include "probability/dice.h"

namespace monopoly::probability {

double pTie() {
  auto pmf = twoD6Pmf();
  double p = 0.0;
  for (int s = 2; s <= 12; ++s) p += pmf[s] * pmf[s];
  return p;
}

double pMuggerWins() {
  auto pmf = twoD6Pmf();
  double p = 0.0;
  for (int hi = 2; hi <= 12; ++hi)
    for (int lo = 2; lo < hi; ++lo)
      p += pmf[hi] * pmf[lo];
  return p;
}

double pMuggeeEscapes() { return pMuggerWins() + pTie(); }

}  // namespace monopoly::probability
