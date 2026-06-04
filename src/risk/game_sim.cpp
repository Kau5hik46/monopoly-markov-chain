#include "risk/game_sim.h"

#include <random>
#include <vector>
#include "domain/square.h"
#include "probability/micro_state.h"
#include "rules/rule_logic.h"
#include "risk/rent_table.h"

namespace monopoly::risk {

namespace {
constexpr int kHospital = 20;  // Free Parking

int sampleLanding(const probability::LandingResolver& resolver, int raw,
                  std::mt19937& rng) {
  auto dist = resolver.resolve(raw);
  std::uniform_real_distribution<double> u(0.0, 1.0);
  double x = u(rng), acc = 0.0;
  for (const auto& lp : dist) {
    acc += lp.prob;
    if (x <= acc) return lp.position;
  }
  return dist.empty() ? raw % probability::kNumPositions : dist.back().position;
}

// Plays one full turn for player p in the cloned state `s`; returns timesRobbed of
// OTHER players is tracked via `robbed`. Two-dice rolls drawn from `rng`.
void playTurn(domain::GameState& s, int p, const rules::RuleConfig& rules,
              const probability::LandingResolver& resolver, std::mt19937& rng,
              std::vector<long>& robbed) {
  std::uniform_int_distribution<int> die(1, 6);
  int streak = 0;
  for (;;) {
    auto& pl = s.player(p);
    const int d1 = die(rng), d2 = die(rng), sum = d1 + d2;
    const bool dbl = (d1 == d2);

    if (pl.inJail) {
      if (dbl) { pl.inJail = false; pl.jailAttempts = 0; }
      else if (++pl.jailAttempts >= rules.jailMaxAttempts) {
        pl.cash -= rules.jailFine; pl.inJail = false; pl.jailAttempts = 0;
      } else {
        return;  // stays in jail, turn ends
      }
      pl.position = sampleLanding(resolver, (10 + sum) % probability::kNumPositions, rng);
      if (pl.position == probability::kJailSentinel) { pl.inJail = true; pl.position = 10; return; }
    } else {
      if (dbl && ++streak >= 3) { pl.inJail = true; pl.position = 10; return; }
      const int oldPos = pl.position;
      const int raw = oldPos + sum;
      pl.position = sampleLanding(resolver, raw % probability::kNumPositions, rng);
      if (pl.position == probability::kJailSentinel) { pl.inJail = true; pl.position = 10; return; }
      if (raw >= probability::kNumPositions) pl.cash += rules.passGoBonus;
      if (pl.position == 0 && rules.landOnGoDoubles) pl.cash += rules.passGoBonus;
    }

    const int pos = pl.position;
    const auto& sq = s.board().at(pos);
    // Rent to an opponent owner.
    if (domain::isPurchasable(sq.type)) {
      const int owner = s.ownerOf(pos);
      if (owner != domain::kUnowned && owner != p && !s.isMortgaged(pos)) {
        const long rent = risk::rentOwed(s, pos, p, sum);
        pl.cash -= rent;
        s.player(owner).cash += rent;
      }
    }
    // Mugging coupling.
    if (rules.muggingEnabled && rules::isMuggingEligible(sq.type)) {
      const int occ = s.occupantAt(pos, /*exclude=*/p);
      if (occ != domain::kUnowned) {
        const int m1 = die(rng) + die(rng), m2 = die(rng) + die(rng);
        if (m1 > m2) {  // mugger (p) wins
          s.player(occ).cash -= rules.muggingAmount;
          pl.cash += rules.muggingAmount;
          s.player(occ).position = kHospital;
          s.player(occ).inJail = false;
          robbed[static_cast<std::size_t>(occ)] += 1;
        } else {  // muggee resists -> mugger to jail, turn ends
          pl.position = 10; pl.inJail = true;
          return;
        }
      }
    }
    if (!(dbl && !pl.inJail)) return;  // no double (or jailed) -> turn ends
  }
}
}  // namespace

GameForecast simulateGame(const domain::GameState& gs, const rules::RuleConfig& rules,
                          int rounds, const probability::LandingResolver& resolver,
                          long trials, unsigned seed) {
  GameForecast fc;
  fc.rounds = rounds;
  const int n = gs.numPlayers();
  fc.players.assign(static_cast<std::size_t>(std::max(0, n)), PlayerOutcome{});
  if (n <= 0 || rounds <= 0 || trials <= 0) return fc;

  std::vector<long> ruinCount(static_cast<std::size_t>(n), 0);
  std::vector<double> cashDelta(static_cast<std::size_t>(n), 0.0);
  std::vector<double> robbedTotal(static_cast<std::size_t>(n), 0.0);
  std::mt19937 rng(seed);

  for (long t = 0; t < trials; ++t) {
    domain::GameState s = gs;  // Prototype clone; ownership/houses shared by value
    std::vector<bool> bankrupt(static_cast<std::size_t>(n), false);
    std::vector<long> robbed(static_cast<std::size_t>(n), 0);

    for (int round = 0; round < rounds; ++round) {
      for (int p = 0; p < n; ++p) {
        if (bankrupt[static_cast<std::size_t>(p)]) continue;
        playTurn(s, p, rules, resolver, rng, robbed);
        for (int q = 0; q < n; ++q)
          if (!bankrupt[static_cast<std::size_t>(q)] && s.player(q).cash < 0) {
            bankrupt[static_cast<std::size_t>(q)] = true;
            ruinCount[static_cast<std::size_t>(q)] += 1;
          }
      }
    }
    for (int p = 0; p < n; ++p) {
      cashDelta[static_cast<std::size_t>(p)] +=
          static_cast<double>(s.player(p).cash - gs.player(p).cash);
      robbedTotal[static_cast<std::size_t>(p)] += robbed[static_cast<std::size_t>(p)];
    }
  }

  const double inv = 1.0 / static_cast<double>(trials);
  for (int p = 0; p < n; ++p) {
    auto& o = fc.players[static_cast<std::size_t>(p)];
    o.ruinProbability = static_cast<double>(ruinCount[static_cast<std::size_t>(p)]) * inv;
    o.expectedCashDelta = cashDelta[static_cast<std::size_t>(p)] * inv;
    o.expectedTimesRobbed = robbedTotal[static_cast<std::size_t>(p)] * inv;
  }
  return fc;
}

}  // namespace monopoly::risk
