#include <gtest/gtest.h>
#include "contracts/option_book.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "engine/executor.h"
#include "engine/name_table.h"
#include "engine/parser.h"
#include "rules/rule_config.h"

using namespace monopoly;
using namespace monopoly::engine;

namespace {
struct Harness {
  domain::Board board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  domain::Decks decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  rules::RuleConfig rules;
  domain::GameState gs{board};
  NameTable names{board};
  Executor exec{gs, decks, rules};
  Harness() {
    run("init 2");
    gs.player(0).position = 18;
    gs.setOwner(24, 1); gs.setOwner(21, 1); gs.setOwner(23, 1);
    gs.setHouses(24, 5);
  }
  CommandResult run(const std::string& line) { return exec.execute(parseLine(line, names)); }
};
}  // namespace

TEST(OptionsExec, InsureOpensBankContract) {
  Harness h;
  auto r = h.run("insure P1 strike 0");
  EXPECT_TRUE(r.ok);
  ASSERT_EQ(h.gs.contracts().size(), 1u);
  EXPECT_EQ(h.gs.contracts()[0].writer, domain::kUnowned);
}

TEST(OptionsExec, GateBlocksMutationWhileMatured) {
  Harness h;
  ASSERT_TRUE(h.run("write P2 -> P1 strike 0 premium 500K").ok);
  contracts::matureOnRoll(h.gs, 0, 3000000);  // mature directly to isolate the gate
  auto blocked = h.run("buy P1 @#1");
  EXPECT_FALSE(blocked.ok);
  EXPECT_FALSE(blocked.effects.empty());
  auto s = h.run("settle all");
  EXPECT_TRUE(s.ok);
  EXPECT_FALSE(contracts::hasMatured(h.gs));
}

TEST(OptionsExec, SettleAllowedThroughGate) {
  Harness h;
  ASSERT_TRUE(h.run("write P2 -> P1 strike 0 premium 500K").ok);
  contracts::matureOnRoll(h.gs, 0, 2000000);
  EXPECT_TRUE(h.run("settle all").ok);
}

TEST(OptionsExec, RollMaturesOpenContractOnInsured) {
  Harness h;
  ASSERT_TRUE(h.run("insure P1 strike 0").ok);
  // P1 at 18 rolling 3,3 lands on 24 (Trafalgar hotel) and pays rent.
  h.run("roll P1 = 3,3");
  EXPECT_TRUE(contracts::hasMatured(h.gs));
}

TEST(OptionsExec, QueryLedgerListsSettledAndOpen) {
  Harness h;
  ASSERT_TRUE(h.run("insure P1 strike 0").ok);
  auto r = h.run("query ledger");
  EXPECT_TRUE(r.ok);
  ASSERT_FALSE(r.effects.empty());
  EXPECT_NE(r.effects.front().text.find("P1"), std::string::npos);  // open row shown
}

TEST(OptionsExec, UndoReversesOpenAndContract) {
  Harness h;
  long cash0 = h.gs.player(0).cash;
  ASSERT_TRUE(h.run("insure P1 strike 0").ok);
  EXPECT_LT(h.gs.player(0).cash, cash0);     // premium left
  ASSERT_TRUE(h.run("undo").ok);
  EXPECT_EQ(h.gs.player(0).cash, cash0);     // premium restored
  EXPECT_TRUE(h.gs.contracts().empty());     // contract gone
}

TEST(OptionsExec, IncomeOptionAccumulatesMaturesAtOwnerTurnAndSettles) {
  Harness h;
  ASSERT_TRUE(h.run("init 1").ok);            // add P3 (now 3 players)
  h.gs.player(2).cash = 50000000;             // fund the writer P3 to cover escrow
  // P3 writes a CALL on P2's income to holder P2 (owner hedges own income); lander = P1.
  ASSERT_TRUE(h.run("write P3 -> P2 call income P2 strike 0 premium 100K landers P1").ok);
  ASSERT_EQ(h.gs.contracts().size(), 1u);
  EXPECT_EQ(h.gs.contracts()[0].underlying, domain::Underlying::Income);

  long holderBefore = h.gs.player(1).cash;    // P2 cash after paying premium
  // P1 rolls 18 -> 24 (2+4) onto P2's hotel and pays rent to P2.
  auto roll1 = h.run("roll P1 = 2,4");
  EXPECT_TRUE(roll1.ok);
  EXPECT_GT(h.gs.contracts()[0].realizedValue, 0);   // income accumulated
  EXPECT_FALSE(contracts::hasMatured(h.gs));          // not until owner's turn

  // It is now P2's (owner) turn — the income option matures and the gate blocks the roll.
  auto roll2 = h.run("roll P2 = 1,2");
  EXPECT_FALSE(roll2.ok);
  EXPECT_TRUE(contracts::hasMatured(h.gs));

  auto s = h.run("settle all");
  EXPECT_TRUE(s.ok);
  EXPECT_GT(h.gs.player(1).cash, holderBefore);       // P2 received rent + option payout
}
