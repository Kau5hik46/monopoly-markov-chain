#pragma once
#include <string>
#include "domain/game_state.h"
#include "probability/landing.h"

namespace monopoly::contracts {

struct OpenResult {
  bool ok = true;
  std::string error;
  int contractId = 0;
  long premium = 0;     // actual premium charged (fair for bank, negotiated for peer)
  long escrow = 0;
  long fairValue = 0;   // engine's fair value at this strike+type (for the echo)
};

struct SettleResult {
  bool ok = true;
  std::string error;
  int settledCount = 0;
  long totalPayout = 0;
};

// Bank-written: premium = fair value at K (call or put) from the loss distribution.
OpenResult openBank(domain::GameState& gs, int holder, int insured,
                    domain::OptionType type, long strike,
                    const probability::LandingResolver& resolver);

// Peer-written: operator-specified premium; escrow = max payoff of the chosen type
// locked from the writer (fails if writer cash < escrow).
OpenResult openPeer(domain::GameState& gs, int writer, int holder, int insured,
                    domain::OptionType type, long strike, long premium,
                    const probability::LandingResolver& resolver);

// Flip every Open liability contract on `insured` to Matured, recording realizedValue.
void matureOnRoll(domain::GameState& gs, int insured, long realizedValue);

// Bank-written income option on owner `owner`. Empty `landers` => auto-detect reachable.
OpenResult openIncomeBank(domain::GameState& gs, int holder, int owner,
                          domain::OptionType type, long strike, std::vector<int> landers,
                          const probability::LandingResolver& resolver);

// Peer-written income option; escrow = conservative bound on total income to `owner`.
OpenResult openIncomePeer(domain::GameState& gs, int writer, int holder, int owner,
                          domain::OptionType type, long strike, long premium,
                          std::vector<int> landers,
                          const probability::LandingResolver& resolver);

// Window accumulation: if `payer` is a referenced lander of an open income contract on
// `owner`, add `rent` to its realizedValue and bump landersRolled.
void accumulateIncome(domain::GameState& gs, int owner, int payer, long rent);

// Turn-aware maturity: when `owner` is about to roll again, mature their open income
// contracts that have seen >= 1 lander roll (the round window has closed).
void matureIncomeOnOwnerTurn(domain::GameState& gs, int owner);

// True if any contract awaits settlement (the no-forget gate predicate).
bool hasMatured(const domain::GameState& gs);

// True if `insured` has an Open liability contract (so a roll should mature it).
bool hasOpenOn(const domain::GameState& gs, int insured);

// Human-readable description of the first matured contract (for the gate message).
std::string firstMaturedSummary(const domain::GameState& gs);

// Settle one matured contract by id, or all of them when allMatured is true.
SettleResult settle(domain::GameState& gs, int id, bool allMatured);

}  // namespace monopoly::contracts
