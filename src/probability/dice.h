#pragma once
#include <array>

namespace monopoly::probability {

// PMF of the sum of two fair 6-sided dice, indexed by sum (0..12).
// pmf[0] = pmf[1] = 0. Sums to 1.
constexpr std::array<double, 13> twoD6Pmf() {
  std::array<double, 13> pmf{};
  for (int a = 1; a <= 6; ++a)
    for (int b = 1; b <= 6; ++b)
      pmf[a + b] += 1.0 / 36.0;
  return pmf;
}

// Probability a single 2d6 roll shows a double (6/36).
constexpr double pDouble() { return 6.0 / 36.0; }

// Mugging contest: mugger and muggee each roll a fresh 2d6; muggee escapes on >=.
double pMuggerWins();     // P(mugger sum  >  muggee sum)
double pTie();            // P(equal sums)
double pMuggeeEscapes();  // P(muggee sum >= mugger sum) = pMuggerWins() + pTie()

}  // namespace monopoly::probability
