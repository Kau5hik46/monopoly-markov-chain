#include "engine/query_service.h"

#include <algorithm>
#include <sstream>
#include <vector>
#include "engine/amount.h"
#include "engine/formatter.h"
#include "pricing/insurance_pricer.h"

namespace monopoly::engine {

namespace {
std::vector<int> rankTop(const probability::PositionVector& v, int k) {
  std::vector<int> order(probability::kNumPositions);
  for (int i = 0; i < probability::kNumPositions; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return v[a] > v[b]; });
  if (static_cast<int>(order.size()) > k) order.resize(static_cast<std::size_t>(k));
  return order;
}
}  // namespace

QueryService::QueryService(const domain::GameState& gs, const domain::Decks& decks,
                           const rules::RuleConfig& rules)
    : gs_(gs), rules_(rules), engine_(gs.board(), decks),
      resolver_(gs.board(), decks) {}

std::string QueryService::advisory(int mover) const {
  if (mover < 0 || mover >= gs_.numPlayers()) return "";
  pricing::PricingConfig cfg;
  cfg.muggingEnabled = rules_.muggingEnabled;
  cfg.muggingAmount = rules_.muggingAmount;
  auto q = pricing::priceNextRoll(gs_, mover, resolver_, cfg);
  std::ostringstream os;
  os << "advisory — P" << (mover + 1) << " (" << gs_.player(mover).name
     << ") next roll:\n"
     << "    expected rent liability : " << formatMoney(q.expectedRent) << "\n"
     << "    max single-roll risk    : " << formatMoney(q.maxRent);
  if (q.maxRentSquare >= 0)
    os << " (" << gs_.board().at(q.maxRentSquare).name << ")";
  os << "\n"
     << "    mugging EV (benefit)    : " << formatMoney(q.muggingExposure) << "\n"
     << "    fair insurance premium  : " << formatMoney(q.fairPremium) << "\n";
  return os.str();
}

CommandResult QueryService::handle(const Command& c) const {
  CommandResult r;
  std::ostringstream os;
  switch (c.query) {
    case QueryKind::State:
      r.add(EffectKind::Query, "\n" + formatStatePanel(gs_));
      return r;
    case QueryKind::Stationary: {
      auto pi = engine_.stationaryByPosition();
      os << "long-run landing probability (top 8):\n";
      os << "    JAIL " << (engine_.jailProbability() * 100) << "%\n";
      for (int p : rankTop(pi, 8))
        os << "    " << gs_.board().at(p).name << " " << (pi[p] * 100) << "%\n";
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Dist: {
      if (c.player < 0 || c.player >= gs_.numPlayers()) { r.fail("unknown player"); return r; }
      auto d = engine_.afterNRolls(gs_.player(c.player).position, std::max(1, c.count));
      os << "P" << (c.player + 1) << " distribution after " << std::max(1, c.count)
         << " rolls (top 6):\n";
      for (int p : rankTop(d, 6))
        os << "    " << gs_.board().at(p).name << " " << (d[p] * 100) << "%\n";
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Risk:
    case QueryKind::Options: {
      if (c.player < 0 || c.player >= gs_.numPlayers()) { r.fail("unknown player"); return r; }
      r.add(EffectKind::Query, "\n" + advisory(c.player));
      return r;
    }
    case QueryKind::Value: {
      if (c.posA < 0) { r.fail("query value expects @square"); return r; }
      auto pi = engine_.stationaryByPosition();
      os << gs_.board().at(c.posA).name << ": long-run landing "
         << (pi[c.posA] * 100) << "%";
      r.add(EffectKind::Query, os.str());
      return r;
    }
  }
  r.fail("unhandled query");
  return r;
}

}  // namespace monopoly::engine
