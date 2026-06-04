#pragma once
#include <string>
#include <vector>

namespace monopoly::engine {

enum class EffectKind {
  Info, Move, PassGo, Buy, Sell, RentPaid, CashTransfer, Mugging, SentToJail,
  SentToHospital, TaxToPot, PotClaimed, HouseBuilt, Mortgage, AirportTravel,
  Bankrupt, JailEnter, JailLeave, Double, Query, Error
};

// One causal side effect of a command, in plain English (the rendered text) plus a
// kind for testing/snapshotting.
struct Effect {
  EffectKind kind;
  std::string text;
};

struct CommandResult {
  bool ok = true;
  std::vector<Effect> effects;
  // Expected follow-up actions the operator should record next (suggested commands).
  std::vector<std::string> prompts;

  void add(EffectKind k, std::string text) {
    effects.push_back({k, std::move(text)});
  }
  void prompt(std::string suggestion) { prompts.push_back(std::move(suggestion)); }
  void fail(std::string text) {
    ok = false;
    effects.push_back({EffectKind::Error, std::move(text)});
  }
};

}  // namespace monopoly::engine
