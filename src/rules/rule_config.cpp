#include "rules/rule_config.h"

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace monopoly::rules {

RuleConfig loadRuleConfig(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open rules file: " + path);
  nlohmann::json j;
  in >> j;

  RuleConfig c;  // start from defaults; override present keys
  if (j.contains("muggingEnabled")) c.muggingEnabled = j["muggingEnabled"].get<bool>();
  if (j.contains("muggingAmount")) c.muggingAmount = j["muggingAmount"].get<long>();
  if (j.contains("hospitalPosition"))
    c.hospitalPosition = j["hospitalPosition"].get<int>();
  if (j.contains("airportTravelEnabled"))
    c.airportTravelEnabled = j["airportTravelEnabled"].get<bool>();
  if (j.contains("freeParkingPotEnabled"))
    c.freeParkingPotEnabled = j["freeParkingPotEnabled"].get<bool>();
  if (j.contains("housesPerIncomeTax"))
    c.housesPerIncomeTax = j["housesPerIncomeTax"].get<int>();
  if (j.contains("housesPerSuperTax"))
    c.housesPerSuperTax = j["housesPerSuperTax"].get<int>();
  if (j.contains("respectHouseSupply"))
    c.respectHouseSupply = j["respectHouseSupply"].get<bool>();
  if (j.contains("evenBuild")) c.evenBuild = j["evenBuild"].get<bool>();
  if (j.contains("jailMaxAttempts")) c.jailMaxAttempts = j["jailMaxAttempts"].get<int>();
  if (j.contains("jailFine")) c.jailFine = j["jailFine"].get<long>();
  if (j.contains("passGoBonus")) c.passGoBonus = j["passGoBonus"].get<long>();
  if (j.contains("landOnGoDoubles")) c.landOnGoDoubles = j["landOnGoDoubles"].get<bool>();
  if (j.contains("startingCash")) c.startingCash = j["startingCash"].get<long>();
  if (j.contains("freeParkingHouseCash"))
    c.freeParkingHouseCash = j["freeParkingHouseCash"].get<long>();
  return c;
}

}  // namespace monopoly::rules
