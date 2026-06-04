#include "engine/session.h"

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace monopoly::engine {

using nlohmann::json;

namespace {
json ruleToJson(const rules::RuleConfig& r) {
  return json{{"muggingEnabled", r.muggingEnabled},
              {"muggingAmount", r.muggingAmount},
              {"hospitalPosition", r.hospitalPosition},
              {"airportTravelEnabled", r.airportTravelEnabled},
              {"freeParkingPotEnabled", r.freeParkingPotEnabled},
              {"housesPerIncomeTax", r.housesPerIncomeTax},
              {"housesPerSuperTax", r.housesPerSuperTax},
              {"respectHouseSupply", r.respectHouseSupply},
              {"jailMaxAttempts", r.jailMaxAttempts},
              {"jailFine", r.jailFine},
              {"passGoBonus", r.passGoBonus},
              {"landOnGoDoubles", r.landOnGoDoubles}};
}
void ruleFromJson(const json& j, rules::RuleConfig& r) {
  r.muggingEnabled = j.value("muggingEnabled", r.muggingEnabled);
  r.muggingAmount = j.value("muggingAmount", r.muggingAmount);
  r.hospitalPosition = j.value("hospitalPosition", r.hospitalPosition);
  r.airportTravelEnabled = j.value("airportTravelEnabled", r.airportTravelEnabled);
  r.freeParkingPotEnabled = j.value("freeParkingPotEnabled", r.freeParkingPotEnabled);
  r.housesPerIncomeTax = j.value("housesPerIncomeTax", r.housesPerIncomeTax);
  r.housesPerSuperTax = j.value("housesPerSuperTax", r.housesPerSuperTax);
  r.respectHouseSupply = j.value("respectHouseSupply", r.respectHouseSupply);
  r.jailMaxAttempts = j.value("jailMaxAttempts", r.jailMaxAttempts);
  r.jailFine = j.value("jailFine", r.jailFine);
  r.passGoBonus = j.value("passGoBonus", r.passGoBonus);
  r.landOnGoDoubles = j.value("landOnGoDoubles", r.landOnGoDoubles);
}
}  // namespace

void saveGame(const domain::GameState& gs, const rules::RuleConfig& rules,
              int nextRoller, const std::string& path) {
  json j;
  j["nextRoller"] = nextRoller;
  j["freeParkingPot"] = gs.freeParkingPot();
  j["bank"] = {{"houses", gs.bank().housesAvailable},
               {"hotels", gs.bank().hotelsAvailable}};
  j["rules"] = ruleToJson(rules);
  j["players"] = json::array();
  for (int i = 0; i < gs.numPlayers(); ++i) {
    const auto& p = gs.player(i);
    j["players"].push_back({{"name", p.name},
                            {"position", p.position},
                            {"cash", p.cash},
                            {"inJail", p.inJail},
                            {"jailAttempts", p.jailAttempts},
                            {"consecutiveDoubles", p.consecutiveDoubles}});
  }
  json owner = json::array(), houses = json::array(), mort = json::array();
  for (int pos = 0; pos < domain::kBoardSize; ++pos) {
    owner.push_back(gs.ownerOf(pos));
    houses.push_back(gs.housesOn(pos));
    mort.push_back(gs.isMortgaged(pos));
  }
  j["owner"] = owner;
  j["houses"] = houses;
  j["mortgaged"] = mort;

  std::ofstream out(path);
  if (!out) throw std::runtime_error("cannot write save file: " + path);
  out << j.dump(2) << "\n";
}

void loadGame(domain::GameState& gs, rules::RuleConfig& rules, int& nextRoller,
              const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open save file: " + path);
  json j;
  in >> j;

  domain::GameState fresh(gs.board());  // same board; rebuild mutable state
  for (const auto& p : j.at("players")) {
    int id = fresh.addPlayer(p.at("name").get<std::string>(), p.at("cash").get<long>());
    fresh.player(id).position = p.at("position").get<int>();
    fresh.player(id).inJail = p.at("inJail").get<bool>();
    fresh.player(id).jailAttempts = p.at("jailAttempts").get<int>();
    fresh.player(id).consecutiveDoubles = p.value("consecutiveDoubles", 0);
  }
  const auto& owner = j.at("owner");
  const auto& houses = j.at("houses");
  const auto& mort = j.at("mortgaged");
  for (int pos = 0; pos < domain::kBoardSize; ++pos) {
    fresh.setOwner(pos, owner[pos].get<int>());
    fresh.setHouses(pos, houses[pos].get<int>());
    fresh.setMortgaged(pos, mort[pos].get<bool>());
  }
  fresh.setFreeParkingPot(j.value("freeParkingPot", 0));
  fresh.bank().housesAvailable = j.at("bank").value("houses", domain::kHouseSupply);
  fresh.bank().hotelsAvailable = j.at("bank").value("hotels", domain::kHotelSupply);

  gs = fresh;
  if (j.contains("rules")) ruleFromJson(j.at("rules"), rules);
  nextRoller = j.value("nextRoller", 0);
}

}  // namespace monopoly::engine
