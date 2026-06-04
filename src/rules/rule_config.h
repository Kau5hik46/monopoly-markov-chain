#pragma once
#include <string>

namespace monopoly::rules {

// Configurable house rules. Defaults are the canonical advisor settings; a config
// file overrides any subset.
struct RuleConfig {
  // Mugging: land on an opponent-occupied eligible square -> 2d6 contest.
  bool muggingEnabled = true;
  long muggingAmount = 500000;     // transferred from muggee to mugger on a win
  int hospitalPosition = 20;       // Free Parking ("hospital") destination for muggee

  // Airport travel: from an owned airport, optionally travel to another owned airport.
  bool airportTravelEnabled = true;

  // Free-Parking house pot: taxes paid to the bank become parked houses.
  bool freeParkingPotEnabled = true;
  int housesPerIncomeTax = 2;
  int housesPerSuperTax = 1;
  bool respectHouseSupply = true;  // draw from the bank's 32/12 supply

  // Jail policy.
  int jailMaxAttempts = 3;
  long jailFine = 500000;

  // Economy.
  long passGoBonus = 2000000;
};

// Fluent builder (Builder pattern).
class RuleConfigBuilder {
 public:
  RuleConfigBuilder& mugging(bool on, long amount = 500000) {
    cfg_.muggingEnabled = on;
    cfg_.muggingAmount = amount;
    return *this;
  }
  RuleConfigBuilder& airportTravel(bool on) {
    cfg_.airportTravelEnabled = on;
    return *this;
  }
  RuleConfigBuilder& freeParkingPot(bool on, int perIncome = 2, int perSuper = 1) {
    cfg_.freeParkingPotEnabled = on;
    cfg_.housesPerIncomeTax = perIncome;
    cfg_.housesPerSuperTax = perSuper;
    return *this;
  }
  RuleConfigBuilder& passGoBonus(long bonus) {
    cfg_.passGoBonus = bonus;
    return *this;
  }
  RuleConfig build() const { return cfg_; }

 private:
  RuleConfig cfg_;
};

// Loads a RuleConfig from JSON; unspecified fields keep their defaults.
RuleConfig loadRuleConfig(const std::string& path);

}  // namespace monopoly::rules
