#include "engine/query_service.h"

#include <algorithm>
#include <sstream>
#include <vector>
#include "engine/amount.h"
#include "engine/formatter.h"
#include "pricing/insurance_pricer.h"
#include "pricing/option_chain.h"
#include "risk/forecast.h"
#include "risk/game_sim.h"

namespace monopoly::engine {

namespace {
std::vector<int> rankTop(const probability::PositionVector& v, int k) {
  std::vector<int> order(probability::kNumPositions);
  for (int i = 0; i < probability::kNumPositions; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return v[a] > v[b]; });
  if (static_cast<int>(order.size()) > k) order.resize(static_cast<std::size_t>(k));
  return order;
}
std::string dots(const std::string& label, std::size_t to) {
  std::string s = " " + label + " ";
  while (s.size() < to) s += ".";
  return s + " ";
}
}  // namespace

QueryService::QueryService(const domain::GameState& gs, const domain::Decks& decks,
                           const rules::RuleConfig& rules, const Palette& pal)
    : gs_(gs), rules_(rules), pal_(pal), engine_(gs.board(), decks),
      resolver_(gs.board(), decks) {}

std::string QueryService::advisory(int mover) const {
  if (mover < 0 || mover >= gs_.numPlayers()) return "";
  pricing::PricingConfig cfg;
  cfg.muggingEnabled = rules_.muggingEnabled;
  cfg.muggingAmount = rules_.muggingAmount;
  auto q = pricing::priceNextRoll(gs_, mover, resolver_, cfg);
  const long cash = gs_.player(mover).cash;

  // Qualitative risk level.
  const bool insolvent = (cash > 0 && q.maxRent > static_cast<double>(cash));
  std::string level;
  std::string (Palette::*color)(const std::string&) const;
  if (insolvent || q.fairPremium > 0.08 * static_cast<double>(std::max<long>(cash, 1))) {
    level = "HIGH"; color = &Palette::red;
  } else if (q.fairPremium > 0.02 * static_cast<double>(std::max<long>(cash, 1))) {
    level = "MED"; color = &Palette::yellow;
  } else {
    level = "LOW"; color = &Palette::green;
  }

  std::ostringstream os;
  std::string title = "ADVISORY  next-to-roll: P" + std::to_string(mover + 1);
  os << sectionHeader(title, (pal_.*color)("RISK: " + level), pal_) << "\n";
  os << dots("Expected rent liability", 33)
     << formatMoney(q.expectedRent) << "\n";
  os << dots("Max single-roll risk", 33) << formatMoney(q.maxRent);
  if (q.maxRentSquare >= 0) os << "  on " << gs_.board().at(q.maxRentSquare).name;
  if (insolvent) os << "  " << pal_.red("[> CASH]");
  os << "\n";
  os << dots("Mugging expected value", 33) << "+" << formatMoney(q.muggingExposure)
     << "  " << pal_.dim("(benefit)") << "\n";
  os << dots("Fair insurance premium", 33) << pal_.bold(formatMoney(q.fairPremium))
     << "\n";

  if (!q.threats.empty()) {
    os << "\n " << padRight("TOP THREATS", 32) << padLeft("LAND%", 7) << "   EXP\n";
    int shown = 0;
    for (const auto& t : q.threats) {
      if (shown++ >= 3) break;
      std::ostringstream pct; pct << (t.landProb * 100);
      std::string name = gs_.board().at(t.square).name + " (" +
                         std::to_string(t.square) + ")";
      os << "  " << shown << ". " << padRight(truncate(name, 28), 29)
         << padLeft(pct.str().substr(0, 5) + "%", 6) << "  "
         << pal_.cyan(formatMoney(t.expectedContribution)) << "\n";
    }
  }
  return os.str();
}

CommandResult QueryService::handle(const Command& c) const {
  CommandResult r;
  std::ostringstream os;
  switch (c.query) {
    case QueryKind::State:
      r.add(EffectKind::Query, formatStatePanel(gs_, -1, pal_));
      return r;
    case QueryKind::Board:
      r.add(EffectKind::Query, formatBoard(gs_, pal_));
      return r;
    case QueryKind::Stationary: {
      auto pi = engine_.stationaryByPosition();
      double jail = engine_.jailProbability();
      auto top = rankTop(pi, 10);
      double maxp = top.empty() ? 1.0 : pi[top[0]];
      os << sectionHeader("STATIONARY  long-run landing", "top 10 + JAIL", pal_) << "\n";
      os << " " << padLeft("#", 3) << "  " << padRight("SQUARE", 26)
         << padLeft("LAND%", 7) << "  BAR\n";
      int rank = 0;
      for (int p : top) {
        ++rank;
        std::ostringstream pct; pct << (pi[p] * 100);
        int bars = static_cast<int>((pi[p] / maxp) * 16.0 + 0.5);
        os << " " << padLeft(std::to_string(rank), 3) << "  "
           << padRight(truncate(gs_.board().at(p).name, 24), 26)
           << padLeft(pct.str().substr(0, 5) + "%", 7) << "  "
           << pal_.cyan(std::string(static_cast<std::size_t>(bars), '#')) << "\n";
      }
      std::ostringstream jp; jp << (jail * 100);
      os << " " << pal_.dim("JAIL (in-jail state, tracked separately)   ")
         << jp.str().substr(0, 5) << "%\n";
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Dist: {
      if (c.player < 0 || c.player >= gs_.numPlayers()) { r.fail("unknown player"); return r; }
      int n = std::max(1, c.count);
      auto d = engine_.afterNRolls(gs_.player(c.player).position, n);
      os << sectionHeader("DISTRIBUTION  P" + std::to_string(c.player + 1) + " in " +
                              std::to_string(n) + " rolls", "top 6", pal_) << "\n";
      for (int p : rankTop(d, 6)) {
        std::ostringstream pct; pct << (d[p] * 100);
        os << "  " << padRight(truncate(gs_.board().at(p).name, 26), 27)
           << padLeft(pct.str().substr(0, 5) + "%", 7) << "\n";
      }
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Risk:
    case QueryKind::Options: {
      if (c.player < 0 || c.player >= gs_.numPlayers()) { r.fail("unknown player"); return r; }
      r.add(EffectKind::Query, advisory(c.player));
      return r;
    }
    case QueryKind::Chain: {
      if (c.player < 0 || c.player >= gs_.numPlayers()) { r.fail("unknown player"); return r; }
      auto chain = pricing::buildOptionChain(gs_, c.player, resolver_);
      os << sectionHeader("OPTION CHAIN  P" + std::to_string(c.player + 1) +
                              " next-roll rent insurance",
                          "E[L] " + formatMoney(chain.expectedLoss), pal_) << "\n";
      if (chain.maxLoss <= 0.0) {
        os << "  " << pal_.dim("no rent liability reachable next roll") << "\n";
        r.add(EffectKind::Query, os.str());
        return r;
      }
      os << " " << padRight("STRIKE (deductible)", 22) << padLeft("P(loss>K)", 11)
         << "   FAIR PREMIUM\n";
      for (const auto& row : chain.rows) {
        std::ostringstream pct; pct << (row.payoutProb * 100);
        os << " " << padRight(formatMoney(row.strike), 22)
           << padLeft(pct.str().substr(0, 5) + "%", 11) << "   "
           << pal_.cyan(formatMoney(row.fairPremium)) << "\n";
      }
      os << " " << pal_.dim("premium = E[max(loss - K, 0)] over the next roll");
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Forecast: {
      if (c.player < 0 || c.player >= gs_.numPlayers()) { r.fail("unknown player"); return r; }
      const int n = std::max(1, c.count);
      auto fc = risk::forecast(gs_, c.player, n, resolver_);
      const long cash = gs_.player(c.player).cash;
      os << sectionHeader("FORECAST  P" + std::to_string(c.player + 1) + " over " +
                              std::to_string(n) + " rolls",
                          "Monte-Carlo", pal_) << "\n";
      os << dots("Expected cumulative rent", 33)
         << formatMoney(fc.expectedCumulativeRent) << "\n";
      std::ostringstream rp; rp << (fc.ruinProbability * 100);
      std::string ruin = rp.str().substr(0, 5) + "%";
      os << dots("Ruin probability", 33)
         << (fc.ruinProbability > 0.25 ? pal_.red(ruin) : ruin)
         << "  (P[cumulative rent > " << formatMoney(static_cast<double>(cash))
         << "])";
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Simulate: {
      if (gs_.numPlayers() == 0) { r.fail("no players — 'init N' first"); return r; }
      const int n = std::max(1, c.count);
      auto fc = risk::simulateGame(gs_, rules_, n, resolver_);
      os << sectionHeader("SIMULATE  " + std::to_string(n) + " rounds, all players",
                          "coupled Monte-Carlo", pal_) << "\n";
      os << " " << padRight("PLAYER", 8) << padLeft("RUIN%", 8) << "   "
         << padRight("E[cash change]", 16) << "robbed\n";
      for (int p = 0; p < gs_.numPlayers(); ++p) {
        const auto& o = fc.players[static_cast<std::size_t>(p)];
        std::ostringstream rp; rp << (o.ruinProbability * 100);
        std::string ruinCell = padLeft(rp.str().substr(0, 5) + "%", 8);
        if (o.ruinProbability > 0.25) ruinCell = pal_.red(ruinCell);
        const double d = o.expectedCashDelta;
        std::string cashCell = padRight((d >= 0 ? "+" : "-") +
                                            formatMoney(d < 0 ? -d : d), 16);
        cashCell = (d >= 0) ? pal_.green(cashCell) : pal_.red(cashCell);
        std::ostringstream rb; rb << o.expectedTimesRobbed;
        os << " " << padRight("P" + std::to_string(p + 1), 8) << ruinCell << "   "
           << cashCell << rb.str().substr(0, 4) << "\n";
      }
      os << " " << pal_.dim("includes mugging displacement & rent; ownership fixed");
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Value: {
      if (c.posA < 0) { r.fail("query value expects @square"); return r; }
      auto pi = engine_.stationaryByPosition();
      std::ostringstream pct; pct << (pi[c.posA] * 100);
      os << gs_.board().at(c.posA).name << ": long-run landing "
         << pct.str().substr(0, 5) << "%";
      r.add(EffectKind::Query, os.str());
      return r;
    }
    case QueryKind::Ledger: {
      auto writerLabel = [](int w) {
        return w == domain::kUnowned ? std::string("BANK") : "P" + std::to_string(w + 1);
      };
      auto typeLabel = [](domain::OptionType t) {
        return t == domain::OptionType::Put ? "PUT" : "CALL";
      };
      os << sectionHeader("OPTION LEDGER", "contracts & P&L", pal_) << "\n";
      os << " open / matured:\n";
      bool anyOpen = false;
      for (const auto& ct : gs_.contracts()) {
        if (ct.status == domain::ContractStatus::Settled) continue;
        anyOpen = true;
        os << "   #" << ct.id << "  P" << (ct.holder + 1) << " <- " << writerLabel(ct.writer)
           << "  " << typeLabel(ct.type) << " on P" << (ct.insured + 1)
           << "  K=" << formatMoney(ct.strike) << " prem=" << formatMoney(ct.premium)
           << " escrow=" << formatMoney(ct.escrow)
           << (ct.status == domain::ContractStatus::Matured ? "  [MATURED]" : "") << "\n";
      }
      if (!anyOpen) os << "   (none)\n";
      os << " settled:\n";
      std::vector<long> net(static_cast<std::size_t>(std::max(1, gs_.numPlayers())), 0);
      for (const auto& e : gs_.ledger()) {
        os << "   #" << e.id << "  " << typeLabel(e.type) << "  payout "
           << formatMoney(e.payout) << " (prem " << formatMoney(e.premium) << ")\n";
        net[static_cast<std::size_t>(e.holder)] += e.payout - e.premium;
        if (e.writer != domain::kUnowned)
          net[static_cast<std::size_t>(e.writer)] += e.premium - e.payout;
      }
      if (gs_.ledger().empty()) os << "   (none)\n";
      os << " net P&L:";
      for (int p = 0; p < gs_.numPlayers(); ++p)
        os << "  P" << (p + 1) << " " << formatMoney(net[static_cast<std::size_t>(p)]);
      r.add(EffectKind::Query, os.str());
      return r;
    }
  }
  r.fail("unhandled query");
  return r;
}

}  // namespace monopoly::engine
