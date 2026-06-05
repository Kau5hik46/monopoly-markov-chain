#include "pricing/option_chain.h"

#include <algorithm>
#include <cmath>
#include <map>
#include "probability/micro_state.h"
#include "risk/rent_table.h"

namespace monopoly::pricing {

namespace {
// Distribution (value -> probability, including the 0 bucket) of rent `lander` pays to
// `owner` on the lander's single next roll.
std::map<long, double> perLanderIncome(const domain::GameState& gs, int owner, int lander,
                                       const probability::LandingResolver& resolver) {
  std::map<long, double> dist;
  const int from = gs.player(lander).position;
  for (int d1 = 1; d1 <= 6; ++d1) {
    for (int d2 = 1; d2 <= 6; ++d2) {
      const int sum = d1 + d2;
      const int raw = (from + sum) % probability::kNumPositions;
      for (const auto& lp : resolver.resolve(raw)) {
        const double w = (1.0 / 36.0) * lp.prob;
        long rent = 0;  // jail (sentinel) or non-owner landing => no rent to owner
        if (lp.position != probability::kJailSentinel &&
            gs.ownerOf(lp.position) == owner && owner != lander &&
            !gs.isMortgaged(lp.position))
          rent = risk::rentOwed(gs, lp.position, lander, sum);
        dist[rent] += w;  // every branch contributes -> per-lander mass sums to 1
      }
    }
  }
  return dist;
}
}  // namespace

namespace {
// Round to a "nice" 1/2/5 x 10^k step covering ~`rows` increments up to `span`.
double niceStep(double span, int rows) {
  if (span <= 0 || rows <= 0) return 0.0;
  const double raw = span / rows;
  const double mag = std::pow(10.0, std::floor(std::log10(raw)));
  const double norm = raw / mag;
  double mult = (norm <= 1.0) ? 1.0 : (norm <= 2.0) ? 2.0 : (norm <= 5.0) ? 5.0 : 10.0;
  return mult * mag;
}
}  // namespace

std::vector<LossOutcome> buildLossDistribution(
    const domain::GameState& gs, int mover,
    const probability::LandingResolver& resolver) {
  const int from = gs.player(mover).position;
  std::vector<LossOutcome> out;
  for (int d1 = 1; d1 <= 6; ++d1) {
    for (int d2 = 1; d2 <= 6; ++d2) {
      const int sum = d1 + d2;
      const int raw = (from + sum) % probability::kNumPositions;
      for (const auto& lp : resolver.resolve(raw)) {
        if (lp.position == probability::kJailSentinel) continue;
        const long rent = risk::rentOwed(gs, lp.position, mover, sum);
        if (rent > 0)
          out.push_back({(1.0 / 36.0) * lp.prob, static_cast<double>(rent)});
      }
    }
  }
  return out;
}

OptionChainResult buildOptionChain(const domain::GameState& gs, int mover,
                                   const probability::LandingResolver& resolver,
                                   int maxRows) {
  auto loss = buildLossDistribution(gs, mover, resolver);
  OptionChainResult res;
  for (const auto& o : loss) {
    res.expectedLoss += o.prob * o.rent;
    res.maxLoss = std::max(res.maxLoss, o.rent);
  }

  auto eval = [&](double k) {
    OptionRow row;
    row.strike = k;
    for (const auto& o : loss) {
      if (o.rent > k) {
        row.payoutProb += o.prob;
        row.fairPremium += o.prob * (o.rent - k);
      }
    }
    return row;
  };

  if (res.maxLoss <= 0.0) {  // nothing to insure
    res.rows.push_back(eval(0.0));
    return res;
  }
  const double step = niceStep(res.maxLoss, std::max(1, maxRows - 1));
  res.rows.push_back(eval(0.0));
  for (double k = step; k < res.maxLoss && static_cast<int>(res.rows.size()) < maxRows;
       k += step)
    res.rows.push_back(eval(k));
  res.rows.push_back(eval(res.maxLoss));  // top strike: zero premium
  return res;
}

double fairPremiumAtStrike(const std::vector<LossOutcome>& loss, double K) {
  double premium = 0.0;
  for (const auto& o : loss)
    if (o.rent > K) premium += o.prob * (o.rent - K);
  return premium;
}

double fairValueAtStrike(const std::vector<LossOutcome>& loss, double K, bool isPut) {
  if (!isPut) return fairPremiumAtStrike(loss, K);
  double mass = 0.0, value = 0.0;
  for (const auto& o : loss) {
    mass += o.prob;
    if (o.rent < K) value += o.prob * (K - o.rent);
  }
  value += (1.0 - mass) * K;  // no-liability mass (L == 0) pays K under a put
  return value;
}

double maxLossOf(const std::vector<LossOutcome>& loss) {
  double m = 0.0;
  for (const auto& o : loss) m = std::max(m, o.rent);
  return m;
}

std::vector<int> reachableLanders(const domain::GameState& gs, int owner,
                                  const probability::LandingResolver& resolver) {
  std::vector<int> out;
  for (int i = 0; i < gs.numPlayers(); ++i) {
    if (i == owner || gs.player(i).inJail) continue;
    auto dist = perLanderIncome(gs, owner, i, resolver);
    bool pays = false;
    for (const auto& kv : dist)
      if (kv.first > 0 && kv.second > 0.0) { pays = true; break; }
    if (pays) out.push_back(i);
  }
  return out;
}

std::vector<LossOutcome> buildIncomeDistribution(
    const domain::GameState& gs, int owner, const std::vector<int>& landers,
    const probability::LandingResolver& resolver) {
  std::map<long, double> total{{0, 1.0}};  // convolution accumulator
  for (int lander : landers) {
    auto per = perLanderIncome(gs, owner, lander, resolver);
    std::map<long, double> next;
    for (const auto& a : total)
      for (const auto& b : per) next[a.first + b.first] += a.second * b.second;
    total.swap(next);
  }
  std::vector<LossOutcome> out;
  for (const auto& kv : total)
    if (kv.first > 0 && kv.second > 0.0)
      out.push_back({kv.second, static_cast<double>(kv.first)});
  return out;
}

}  // namespace monopoly::pricing
