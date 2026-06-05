#include "contracts/option_book.h"

#include <algorithm>
#include "pricing/option_chain.h"

namespace monopoly::contracts {

namespace {
bool validPlayer(const domain::GameState& gs, int id) {
  return id >= 0 && id < gs.numPlayers();
}
long lround(double d) { return static_cast<long>(d < 0 ? d - 0.5 : d + 0.5); }

// Fair value for a liability option of the given type at strike K.
long fairValueOf(const std::vector<pricing::LossOutcome>& loss, domain::OptionType type,
                 long strike) {
  return lround(pricing::fairValueAtStrike(loss, static_cast<double>(strike),
                                           type == domain::OptionType::Put));
}
// Peer escrow = max payoff of the chosen type.
long escrowOf(const std::vector<pricing::LossOutcome>& loss, domain::OptionType type,
              long strike) {
  if (type == domain::OptionType::Put) return strike;             // payoff peaks at U=0
  return std::max(0L, lround(pricing::maxLossOf(loss)) - strike);  // call
}
long payoffOf(const domain::OptionContract& c) {
  return (c.type == domain::OptionType::Call) ? std::max(c.realizedValue - c.strike, 0L)
                                              : std::max(c.strike - c.realizedValue, 0L);
}
}  // namespace

OpenResult openBank(domain::GameState& gs, int holder, int insured,
                    domain::OptionType type, long strike,
                    const probability::LandingResolver& resolver) {
  OpenResult r;
  if (!validPlayer(gs, holder) || !validPlayer(gs, insured)) {
    r.ok = false; r.error = "unknown player"; return r;
  }
  if (strike < 0) { r.ok = false; r.error = "strike must be >= 0"; return r; }
  auto loss = pricing::buildLossDistribution(gs, insured, resolver);
  const long premium = fairValueOf(loss, type, strike);

  gs.player(holder).cash -= premium;  // premium to the bank (implicit)
  domain::OptionContract c;
  c.writer = domain::kUnowned; c.holder = holder; c.insured = insured;
  c.type = type; c.underlying = domain::Underlying::Liability;
  c.strike = strike; c.premium = premium; c.escrow = 0;
  c.status = domain::ContractStatus::Open;
  gs.addContract(c);

  r.contractId = gs.contracts().back().id;
  r.premium = premium; r.fairValue = premium;
  return r;
}

OpenResult openPeer(domain::GameState& gs, int writer, int holder, int insured,
                    domain::OptionType type, long strike, long premium,
                    const probability::LandingResolver& resolver) {
  OpenResult r;
  if (!validPlayer(gs, writer) || !validPlayer(gs, holder) || !validPlayer(gs, insured)) {
    r.ok = false; r.error = "unknown player"; return r;
  }
  if (writer == holder) { r.ok = false; r.error = "writer and holder must differ"; return r; }
  if (strike < 0) { r.ok = false; r.error = "strike must be >= 0"; return r; }
  if (premium < 0) { r.ok = false; r.error = "premium must be >= 0"; return r; }

  auto loss = pricing::buildLossDistribution(gs, insured, resolver);
  const long escrow = escrowOf(loss, type, strike);
  if (gs.player(writer).cash < escrow) {
    r.ok = false; r.error = "writer cannot cover escrow of " + std::to_string(escrow);
    return r;
  }

  gs.player(holder).cash -= premium;
  gs.player(writer).cash += premium;
  gs.player(writer).cash -= escrow;  // locked

  domain::OptionContract c;
  c.writer = writer; c.holder = holder; c.insured = insured;
  c.type = type; c.underlying = domain::Underlying::Liability;
  c.strike = strike; c.premium = premium; c.escrow = escrow;
  c.status = domain::ContractStatus::Open;
  gs.addContract(c);

  r.contractId = gs.contracts().back().id;
  r.premium = premium; r.escrow = escrow;
  r.fairValue = fairValueOf(loss, type, strike);
  return r;
}

void matureOnRoll(domain::GameState& gs, int insured, long realizedValue) {
  for (auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Open &&
        c.underlying == domain::Underlying::Liability && c.insured == insured) {
      c.status = domain::ContractStatus::Matured;
      c.realizedValue = realizedValue;
    }
}

bool hasMatured(const domain::GameState& gs) {
  for (const auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Matured) return true;
  return false;
}

bool hasOpenOn(const domain::GameState& gs, int insured) {
  for (const auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Open &&
        c.underlying == domain::Underlying::Liability && c.insured == insured)
      return true;
  return false;
}

std::string firstMaturedSummary(const domain::GameState& gs) {
  for (const auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Matured) {
      const long raw = payoffOf(c);
      const long cap = (c.writer == domain::kUnowned) ? raw : c.escrow;
      const long payout = std::min(raw, cap);
      return "#" + std::to_string(c.id) +
             (c.type == domain::OptionType::Put ? " PUT" : " CALL") + " (P" +
             std::to_string(c.insured + 1) + " value " + std::to_string(c.realizedValue) +
             ", strike " + std::to_string(c.strike) + " -> payout " +
             std::to_string(payout) + ")";
    }
  return "";
}

SettleResult settle(domain::GameState& gs, int id, bool allMatured) {
  SettleResult r;
  bool any = false;
  for (auto& c : gs.contracts()) {
    if (c.status != domain::ContractStatus::Matured) continue;
    if (!allMatured && c.id != id) continue;
    any = true;
    const long raw = payoffOf(c);
    const long cap = (c.writer == domain::kUnowned) ? raw : c.escrow;  // bank uncapped
    const long payout = std::min(raw, cap);

    if (c.writer == domain::kUnowned) {       // bank pays directly
      gs.player(c.holder).cash += payout;
    } else {                                   // peer: release escrow, pay holder
      gs.player(c.holder).cash += payout;
      gs.player(c.writer).cash += (c.escrow - payout);
    }
    domain::LedgerEntry le;
    le.id = c.id; le.writer = c.writer; le.holder = c.holder; le.insured = c.insured;
    le.type = c.type; le.underlying = c.underlying;
    le.strike = c.strike; le.premium = c.premium; le.escrow = c.escrow; le.payout = payout;
    gs.ledger().push_back(le);
    c.status = domain::ContractStatus::Settled;
    r.settledCount += 1;
    r.totalPayout += payout;
  }
  if (!any) { r.ok = false; r.error = "no matured contract to settle"; }
  return r;
}

}  // namespace monopoly::contracts
