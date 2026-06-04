#include "engine/executor.h"

#include <sstream>
#include <string>
#include "domain/square.h"
#include "engine/amount.h"
#include "engine/help.h"
#include "engine/session.h"
#include "risk/rent_table.h"
#include "rules/rule_logic.h"

namespace monopoly::engine {

using domain::SquareType;

Executor::Executor(domain::GameState& gs, const domain::Decks& decks,
                   rules::RuleConfig& rules, const Palette& pal)
    : gs_(gs), rules_(rules), pal_(pal), query_(gs, decks, rules, pal) {}

void Executor::snapshot() {
  history_.push_back(gs_);
  if (history_.size() > 200) history_.erase(history_.begin());
}

bool Executor::validPlayer(int id, CommandResult& r) const {
  if (id < 0 || id >= gs_.numPlayers()) {
    r.fail("unknown player P" + std::to_string(id + 1));
    return false;
  }
  return true;
}

void Executor::resolveLanding(int player, int pos, int arrivalSum, CommandResult& r) {
  const auto& sq = gs_.board().at(pos);
  if (sq.type == SquareType::GoToJail) {
    auto& pl = gs_.player(player);
    pl.position = 10;
    pl.inJail = true;
    pl.consecutiveDoubles = 0;
    r.add(EffectKind::SentToJail, "P" + std::to_string(player + 1) + " sent to JAIL");
    return;
  }
  const std::string P = "P" + std::to_string(player + 1);
  // Rent to an opponent owner, or a buy opportunity on an unowned property.
  if (domain::isPurchasable(sq.type)) {
    const int owner = gs_.ownerOf(pos);
    if (owner == domain::kUnowned) {
      r.prompt("buy " + P + " @#" + std::to_string(pos) + "   (" + sq.name + ", " +
               formatMoney(static_cast<double>(sq.price)) + ")  — or skip");
    } else if (owner != player && !gs_.isMortgaged(pos)) {
      long rent = risk::rentOwed(gs_, pos, player, arrivalSum);
      gs_.player(player).cash -= rent;
      gs_.player(owner).cash += rent;
      r.add(EffectKind::RentPaid, P + " pays rent " +
                                      formatMoney(static_cast<double>(rent)) + " to P" +
                                      std::to_string(owner + 1));
    }
    // Optional airport travel from an owned airport (needs >= 2 owned airports).
    if (sq.type == SquareType::Station && owner == player &&
        rules_.airportTravelEnabled &&
        gs_.countOwnedInGroup(player, domain::ColorGroup::Station) >= 2) {
      r.prompt("airport " + P + " @#" + std::to_string(pos) +
               " -> @<otherOwnedAirport>   (optional, skips next turn)");
    }
  }
  // Mugging opportunity (in-jail players are excluded by occupantAt).
  if (rules_.muggingEnabled && rules::isMuggingEligible(sq.type)) {
    int occ = gs_.occupantAt(pos, /*exclude=*/player);
    if (occ != domain::kUnowned) {
      r.add(EffectKind::Mugging, "MUGGING: " + P + " lands on P" +
                                     std::to_string(occ + 1) + " — contest required");
      r.prompt("mug " + P + " vs P" + std::to_string(occ + 1) +
               " = <muggerTotal>:<muggeeTotal>");
    }
  }
  if (sq.type == SquareType::IncomeTax)
    r.prompt("tax " + P + " = <amount> @INCOME");
  if (sq.type == SquareType::SuperTax)
    r.prompt("tax " + P + " = <amount> @SUPER");
  if (sq.type == SquareType::Chance || sq.type == SquareType::CommunityChest)
    r.prompt("card " + P + " : GO|JAIL|BACK3|STATION|UTILITY|@SQ|+amt|-amt");
  if (sq.type == SquareType::FreeParking && gs_.freeParkingPot() > 0)
    r.prompt("claim " + P + "   (free-parking pot: " +
             std::to_string(gs_.freeParkingPot()) + " houses)");
}

CommandResult Executor::execute(const Command& c) {
  CommandResult r;
  switch (c.kind) {
    case CommandKind::None:
      return r;
    case CommandKind::Invalid:
      r.fail(c.error);
      return r;
    case CommandKind::Help:
      r.add(EffectKind::Query, manPage(pal_));
      return r;
    case CommandKind::Quit:
      r.add(EffectKind::Info, "bye");
      return r;
    case CommandKind::Query:
      return query_.handle(c);
    case CommandKind::Save:
      try {
        saveGame(gs_, rules_, nextRoller_, c.name);
        r.add(EffectKind::Info, "saved game to " + c.name);
      } catch (const std::exception& e) { r.fail(e.what()); }
      return r;
    case CommandKind::Load:
      try {
        snapshot();  // allow undo of a load
        loadGame(gs_, rules_, nextRoller_, c.name);
        lastMover_ = -1;
        r.add(EffectKind::Info, "loaded game from " + c.name);
      } catch (const std::exception& e) {
        if (!history_.empty()) { gs_ = history_.back(); history_.pop_back(); }
        r.fail(e.what());
      }
      return r;
    case CommandKind::Undo:
      if (history_.empty()) { r.fail("nothing to undo"); return r; }
      gs_ = history_.back();
      history_.pop_back();
      r.add(EffectKind::Info, "undid last command");
      return r;
    default:
      break;
  }

  snapshot();  // mutating commands from here

  switch (c.kind) {
    case CommandKind::Init: {
      for (int i = 0; i < c.count; ++i)
        gs_.addPlayer("P" + std::to_string(gs_.numPlayers() + 1), kStartingCash);
      r.add(EffectKind::Info, "started game with " + std::to_string(c.count) +
                                  " players, each " + formatMoney(kStartingCash));
      lastMover_ = 0;
      nextRoller_ = 0;  // P1 rolls first
      break;
    }
    case CommandKind::Roll: {
      if (!validPlayer(c.player, r)) break;
      if (c.player != nextRoller_) {
        r.fail("out of turn — it is P" + std::to_string(nextRoller_ + 1) +
               "'s roll (use that, or 'cash'/'jail' to correct state)");
        break;
      }
      auto& pl = gs_.player(c.player);
      const int sum = c.die1 + c.die2;
      const bool dbl = (c.die1 == c.die2);
      const bool wasInJail = pl.inJail;
      if (wasInJail) {
        if (dbl) {
          pl.inJail = false; pl.jailAttempts = 0;
          r.add(EffectKind::JailLeave, "P" + std::to_string(c.player + 1) +
                                           " rolls a double and leaves jail");
        } else {
          ++pl.jailAttempts;
          if (pl.jailAttempts >= rules_.jailMaxAttempts) {
            pl.cash -= rules_.jailFine; pl.inJail = false; pl.jailAttempts = 0;
            r.add(EffectKind::JailLeave, "P" + std::to_string(c.player + 1) +
                                             " pays fine " +
                                             formatMoney(rules_.jailFine) +
                                             " and leaves jail");
          } else {
            r.add(EffectKind::Info, "P" + std::to_string(c.player + 1) +
                                        " stays in jail (attempt " +
                                        std::to_string(pl.jailAttempts) + ")");
            lastMover_ = c.player;
            nextRoller_ = (c.player + 1) % gs_.numPlayers();
            break;
          }
        }
      }
      if (dbl && !wasInJail) {
        if (++pl.consecutiveDoubles >= 3) {
          pl.position = 10; pl.inJail = true; pl.consecutiveDoubles = 0;
          r.add(EffectKind::SentToJail,
                "P" + std::to_string(c.player + 1) + " — third double, off to JAIL");
          lastMover_ = c.player;
          nextRoller_ = (c.player + 1) % gs_.numPlayers();
          break;
        }
      } else if (!dbl) {
        pl.consecutiveDoubles = 0;
      }
      const int oldPos = pl.position;
      const int raw = oldPos + sum;
      pl.position = raw % domain::kBoardSize;
      r.add(EffectKind::Move, "P" + std::to_string(c.player + 1) + " rolls " +
                                  std::to_string(c.die1) + "+" +
                                  std::to_string(c.die2) + " to " +
                                  gs_.board().at(pl.position).name + " (" +
                                  std::to_string(pl.position) + ")");
      if (raw >= domain::kBoardSize) {
        pl.cash += rules_.passGoBonus;
        r.add(EffectKind::PassGo, "P" + std::to_string(c.player + 1) +
                                      " passes GO, collects " +
                                      formatMoney(rules_.passGoBonus));
      }
      // House rule: landing exactly on GO pays the salary a second time (2x total).
      if (pl.position == 0 && rules_.landOnGoDoubles) {
        pl.cash += rules_.passGoBonus;
        r.add(EffectKind::PassGo, "P" + std::to_string(c.player + 1) +
                                      " lands on GO — double salary, +" +
                                      formatMoney(rules_.passGoBonus));
      }
      const bool inJailBefore = pl.inJail;
      resolveLanding(c.player, pl.position, sum, r);
      const bool rollsAgain = dbl && !pl.inJail && !inJailBefore;
      if (rollsAgain) {
        r.add(EffectKind::Double, "rolled a double — roll again");
        r.prompt("roll P" + std::to_string(c.player + 1) + " = d1,d2   (rolls again)");
      }
      lastMover_ = c.player;
      nextRoller_ = rollsAgain ? c.player : (c.player + 1) % gs_.numPlayers();
      break;
    }
    case CommandKind::Buy: {
      if (!validPlayer(c.player, r)) break;
      if (gs_.ownerOf(c.posA) != domain::kUnowned) { r.fail("already owned"); break; }
      long price = c.hasAmount ? c.amount : gs_.board().at(c.posA).price;
      gs_.player(c.player).cash -= price;
      gs_.setOwner(c.posA, c.player);
      r.add(EffectKind::Buy, "P" + std::to_string(c.player + 1) + " buys " +
                                 gs_.board().at(c.posA).name + " for " +
                                 formatMoney(static_cast<double>(price)));
      lastMover_ = c.player;
      break;
    }
    case CommandKind::Sell: {
      if (!validPlayer(c.player, r) || !validPlayer(c.player2, r)) break;
      if (gs_.ownerOf(c.posA) != c.player) { r.fail("seller does not own it"); break; }
      long price = c.hasAmount ? c.amount : gs_.board().at(c.posA).price;
      gs_.player(c.player).cash += price;
      gs_.player(c.player2).cash -= price;
      gs_.setOwner(c.posA, c.player2);
      r.add(EffectKind::Sell, "P" + std::to_string(c.player + 1) + " sells " +
                                  gs_.board().at(c.posA).name + " to P" +
                                  std::to_string(c.player2 + 1) + " for " +
                                  formatMoney(static_cast<double>(price)));
      break;
    }
    case CommandKind::Rent: {
      if (!validPlayer(c.player, r) || !validPlayer(c.player2, r)) break;
      long amt = c.hasAmount ? c.amount : risk::rentOwed(gs_, c.posA, c.player, 7);
      gs_.player(c.player).cash -= amt;
      gs_.player(c.player2).cash += amt;
      r.add(EffectKind::RentPaid, "P" + std::to_string(c.player + 1) + " pays " +
                                      formatMoney(static_cast<double>(amt)) + " to P" +
                                      std::to_string(c.player2 + 1));
      break;
    }
    case CommandKind::Build: {
      if (!validPlayer(c.player, r)) break;
      const auto& sq = gs_.board().at(c.posA);
      if (sq.type != SquareType::Street) { r.fail("can only build on streets"); break; }
      int h = gs_.housesOn(c.posA);
      int nh = c.sign ? h + c.count : h - c.count;
      if (nh < 0 || nh > 5) { r.fail("house count out of range (0..5)"); break; }
      const int delta = nh - h;
      if (delta > 0 && rules_.freeParkingPotEnabled && gs_.bank().housesAvailable < delta) {
        r.fail("bank is out of houses");
        break;
      }
      gs_.setHouses(c.posA, nh);
      gs_.bank().housesAvailable -= delta;
      long cost = static_cast<long>(delta) * sq.houseCost;
      gs_.player(c.player).cash -= cost;
      r.add(EffectKind::HouseBuilt, "P" + std::to_string(c.player + 1) +
                                        (delta > 0 ? " builds " : " sells ") +
                                        std::to_string(delta > 0 ? delta : -delta) +
                                        " house(s) on " + sq.name + " (now " +
                                        std::to_string(nh) + ")");
      break;
    }
    case CommandKind::Mortgage:
    case CommandKind::Unmortgage: {
      if (!validPlayer(c.player, r)) break;
      const bool m = (c.kind == CommandKind::Mortgage);
      gs_.setMortgaged(c.posA, m);
      long mv = gs_.board().at(c.posA).mortgage;
      gs_.player(c.player).cash += m ? mv : -mv;
      r.add(EffectKind::Mortgage, "P" + std::to_string(c.player + 1) +
                                      (m ? " mortgages " : " unmortgages ") +
                                      gs_.board().at(c.posA).name);
      break;
    }
    case CommandKind::Tax: {
      if (!validPlayer(c.player, r)) break;
      gs_.player(c.player).cash -= c.amount;
      r.add(EffectKind::CashTransfer, "P" + std::to_string(c.player + 1) + " pays " +
                                          c.name + " tax " +
                                          formatMoney(static_cast<double>(c.amount)));
      SquareType tt = (c.name == "SUPER") ? SquareType::SuperTax : SquareType::IncomeTax;
      int houses = rules::freeParkingHousesForTax(tt, rules_);
      if (houses > 0) {
        if (rules_.respectHouseSupply)
          houses = std::min(houses, gs_.bank().housesAvailable);
        gs_.bank().housesAvailable -= houses;
        gs_.setFreeParkingPot(gs_.freeParkingPot() + houses);
        r.add(EffectKind::TaxToPot, std::to_string(houses) +
                                        " house(s) added to the free-parking pot (now " +
                                        std::to_string(gs_.freeParkingPot()) + ")");
      }
      break;
    }
    case CommandKind::Jail: {
      if (!validPlayer(c.player, r)) break;
      auto& pl = gs_.player(c.player);
      pl.inJail = c.sign;
      pl.jailAttempts = 0;
      if (c.sign) pl.position = 10;
      r.add(c.sign ? EffectKind::JailEnter : EffectKind::JailLeave,
            "P" + std::to_string(c.player + 1) + (c.sign ? " enters jail" : " leaves jail"));
      break;
    }
    case CommandKind::Mug: {
      if (!validPlayer(c.player, r) || !validPlayer(c.player2, r)) break;
      auto res = rules::resolveMugging(c.die1, c.die3);
      if (res == rules::MuggingResult::MuggerWins) {
        gs_.player(c.player2).cash -= rules_.muggingAmount;
        gs_.player(c.player).cash += rules_.muggingAmount;
        gs_.player(c.player2).position = rules_.hospitalPosition;
        gs_.player(c.player2).inJail = false;
        r.add(EffectKind::Mugging, "P" + std::to_string(c.player + 1) +
                                       " wins the mugging: takes " +
                                       formatMoney(rules_.muggingAmount) + " from P" +
                                       std::to_string(c.player2 + 1));
        r.add(EffectKind::SentToHospital, "P" + std::to_string(c.player2 + 1) +
                                              " sent to Free Parking (hospital)");
      } else {
        gs_.player(c.player).position = 10;
        gs_.player(c.player).inJail = true;
        r.add(EffectKind::SentToJail, "P" + std::to_string(c.player2 + 1) +
                                          " resists — P" + std::to_string(c.player + 1) +
                                          " goes to JAIL");
      }
      break;
    }
    case CommandKind::Airport: {
      if (!validPlayer(c.player, r)) break;
      if (!rules_.airportTravelEnabled) { r.fail("airport travel is off"); break; }
      if (!rules::canTravelBetweenAirports(gs_, c.player, c.posA, c.posB)) {
        r.fail("must own both airports to travel between them");
        break;
      }
      gs_.player(c.player).position = c.posB;
      r.add(EffectKind::AirportTravel, "P" + std::to_string(c.player + 1) +
                                           " travels to " + gs_.board().at(c.posB).name +
                                           " (skips next turn)");
      break;
    }
    case CommandKind::Claim: {
      if (!validPlayer(c.player, r)) break;
      int pot = gs_.freeParkingPot();
      if (pot <= 0) { r.add(EffectKind::Info, "free-parking pot is empty"); break; }
      auto cr = rules::claimFreeParkingPot(gs_, c.player);
      std::string msg = "P" + std::to_string(c.player + 1) + " claims " +
                        std::to_string(pot) + " houses: placed " +
                        std::to_string(cr.placed) + " on monopolies";
      if (cr.cashed > 0) {
        long cash = static_cast<long>(cr.cashed) * kFreeParkingHouseCash;
        gs_.player(c.player).cash += cash;
        msg += ", " + std::to_string(cr.cashed) + " -> cash " +
               formatMoney(static_cast<double>(cash));
      }
      r.add(EffectKind::PotClaimed, msg);
      break;
    }
    case CommandKind::Cash: {
      if (!validPlayer(c.player, r)) break;
      gs_.player(c.player).cash += c.sign ? c.amount : -c.amount;
      r.add(EffectKind::CashTransfer, "P" + std::to_string(c.player + 1) +
                                          (c.sign ? " +" : " -") +
                                          formatMoney(static_cast<double>(c.amount)));
      break;
    }
    case CommandKind::Card: {
      if (!validPlayer(c.player, r)) break;
      auto& pl = gs_.player(c.player);
      if (c.name == "JAIL") {
        pl.position = 10; pl.inJail = true; pl.consecutiveDoubles = 0;
        r.add(EffectKind::SentToJail,
              "P" + std::to_string(c.player + 1) + " card: go to JAIL");
        lastMover_ = c.player;
        break;
      }
      if (c.name == "MONEY") {
        const long delta = c.sign ? c.amount : -c.amount;
        pl.cash += delta;
        r.add(EffectKind::CashTransfer, "P" + std::to_string(c.player + 1) +
                                            " card: " + (c.sign ? "+" : "-") +
                                            formatMoney(static_cast<double>(c.amount)));
        lastMover_ = c.player;
        break;
      }
      const int oldPos = pl.position;
      int target = oldPos;
      bool allowGo = true;
      if (c.name == "GO") target = 0;
      else if (c.name == "ADVANCE") target = c.posA;
      else if (c.name == "BACK3") { target = (oldPos - 3 + domain::kBoardSize) % domain::kBoardSize; allowGo = false; }
      else if (c.name == "STATION") target = gs_.board().nearestForward(oldPos, SquareType::Station);
      else if (c.name == "UTILITY") target = gs_.board().nearestForward(oldPos, SquareType::Utility);
      else { r.fail("unknown card effect"); break; }
      const bool passed = allowGo && target < oldPos;
      pl.position = target;
      r.add(EffectKind::Move, "P" + std::to_string(c.player + 1) +
                                  " card: advance to " + gs_.board().at(target).name);
      if (passed) {
        pl.cash += rules_.passGoBonus;
        r.add(EffectKind::PassGo, "passes GO, collects " + formatMoney(rules_.passGoBonus));
      }
      resolveLanding(c.player, target, 7, r);
      lastMover_ = c.player;
      break;
    }
    case CommandKind::Trade: {
      if (!validPlayer(c.player, r) || !validPlayer(c.player2, r)) break;
      bool ok = true; std::string err;
      for (int sq : c.squaresA)
        if (gs_.ownerOf(sq) != c.player) { ok = false; err = "P" + std::to_string(c.player + 1) + " does not own " + gs_.board().at(sq).name; }
      for (int sq : c.squaresB)
        if (gs_.ownerOf(sq) != c.player2) { ok = false; err = "P" + std::to_string(c.player2 + 1) + " does not own " + gs_.board().at(sq).name; }
      if (!ok) { r.fail(err); break; }
      for (int sq : c.squaresA) gs_.setOwner(sq, c.player2);
      for (int sq : c.squaresB) gs_.setOwner(sq, c.player);
      gs_.player(c.player).cash += c.amountB - c.amountA;
      gs_.player(c.player2).cash += c.amountA - c.amountB;
      r.add(EffectKind::Sell,
            "trade P" + std::to_string(c.player + 1) + " <-> P" +
                std::to_string(c.player2 + 1) + ": " +
                std::to_string(c.squaresA.size()) + " prop/" + formatMoney(c.amountA) +
                " <-> " + std::to_string(c.squaresB.size()) + " prop/" +
                formatMoney(c.amountB));
      break;
    }
    case CommandKind::Rules: {
      bool* target = nullptr;
      if (c.name == "mugging") target = &rules_.muggingEnabled;
      else if (c.name == "airport") target = &rules_.airportTravelEnabled;
      else if (c.name == "pot" || c.name == "freeparking")
        target = &rules_.freeParkingPotEnabled;
      if (!target) { r.fail("unknown rule: " + c.name); break; }
      *target = c.flag;
      r.add(EffectKind::Info, "rule '" + c.name + "' " + (c.flag ? "on" : "off"));
      break;
    }
    default:
      r.fail("command not implemented");
      break;
  }
  if (!r.ok && !history_.empty()) {  // roll back a failed mutation
    gs_ = history_.back();
    history_.pop_back();
  }
  return r;
}

}  // namespace monopoly::engine
