#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "engine/executor.h"
#include "engine/parser.h"
#include "rules/rule_config.h"

using namespace monopoly;
using engine::Command;
using engine::Executor;

namespace {
struct Harness {
  domain::Board board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  domain::Decks decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  rules::RuleConfig rules;
  domain::GameState gs{board};
  engine::NameTable names{board};
  Executor exec{gs, decks, rules};
  engine::CommandResult run(const std::string& line) {
    return exec.execute(engine::parseLine(line, names));
  }
};
}  // namespace

TEST(Executor, InitCreatesPlayers) {
  Harness h;
  auto r = h.run("init 3");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(h.gs.numPlayers(), 3);
  EXPECT_EQ(h.gs.player(0).cash, engine::kStartingCash);
}

TEST(Executor, BuyThenRollPaysRent) {
  Harness h;
  h.run("init 2");
  h.run("buy P2 @#24");           // P2 owns Trafalgar Square (24)
  h.run("build P2 @#24 + 0");     // no-op build is rejected; skip
  // Put P1 six behind so a roll of 6 lands on 24.
  h.gs.player(0).position = 18;
  long before = h.gs.player(0).cash;
  auto r = h.run("roll P1 = 2,4");  // sum 6 -> 24
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(h.gs.player(0).position, 24);
  EXPECT_LT(h.gs.player(0).cash, before);  // paid rent
}

TEST(Executor, UndoRestoresState) {
  Harness h;
  h.run("init 2");
  long before = h.gs.player(0).cash;
  h.run("cash P1 -= 1M");
  EXPECT_EQ(h.gs.player(0).cash, before - 1000000);
  auto u = h.run("undo");
  EXPECT_TRUE(u.ok);
  EXPECT_EQ(h.gs.player(0).cash, before);
}

TEST(Executor, TaxFeedsFreeParkingPot) {
  Harness h;
  h.run("init 2");
  h.run("tax P1 = 2M @INCOME");
  EXPECT_EQ(h.gs.freeParkingPot(), 2);  // income tax -> 2 houses
  h.run("tax P1 = 1M @SUPER");
  EXPECT_EQ(h.gs.freeParkingPot(), 3);  // + super tax 1
}

TEST(Executor, MuggingResolution) {
  Harness h;
  h.run("init 2");
  long bob0 = h.gs.player(1).cash;
  auto win = h.run("mug P1 vs P2 = 9:5");   // P1 (mugger) wins
  EXPECT_TRUE(win.ok);
  EXPECT_EQ(h.gs.player(1).cash, bob0 - h.rules.muggingAmount);
  EXPECT_EQ(h.gs.player(1).position, h.rules.hospitalPosition);
  auto lose = h.run("mug P1 vs P2 = 4:10");  // muggee resists -> P1 to jail
  EXPECT_TRUE(lose.ok);
  EXPECT_TRUE(h.gs.player(0).inJail);
}

TEST(Executor, GoToJailSquareJails) {
  Harness h;
  h.run("init 1");
  h.gs.player(0).position = 28;       // 2 behind GO_TO_JAIL(30)
  auto r = h.run("roll P1 = 1,1");    // sum 2 -> 30 -> jail
  EXPECT_TRUE(h.gs.player(0).inJail);
  EXPECT_EQ(h.gs.player(0).position, 10);
}

TEST(Executor, CardAdvanceToGoCollectsBonus) {
  Harness h;
  h.run("init 1");
  h.gs.player(0).position = 30;       // somewhere past GO
  long before = h.gs.player(0).cash;
  auto r = h.run("card P1 : GO");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(h.gs.player(0).position, 0);
  EXPECT_EQ(h.gs.player(0).cash, before + h.rules.passGoBonus);
}

TEST(Executor, CardGoToJail) {
  Harness h;
  h.run("init 1");
  h.run("card P1 : JAIL");
  EXPECT_TRUE(h.gs.player(0).inJail);
  EXPECT_EQ(h.gs.player(0).position, 10);
}

TEST(Executor, TradeSwapsPropertyAndCash) {
  Harness h;
  h.run("init 2");
  h.run("buy P1 @#24");               // P1 owns Trafalgar
  h.run("buy P2 @#21");               // P2 owns London Eye
  long c1 = h.gs.player(0).cash, c2 = h.gs.player(1).cash;
  auto r = h.run("trade P1 <-> P2 : @#24 <-> @#21 1M");  // P1 gives 24, P2 gives 21 + 1M
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(h.gs.ownerOf(24), 1);     // now P2
  EXPECT_EQ(h.gs.ownerOf(21), 0);     // now P1
  EXPECT_EQ(h.gs.player(0).cash, c1 + 1000000);
  EXPECT_EQ(h.gs.player(1).cash, c2 - 1000000);
}

TEST(Executor, TradeRejectsUnownedProperty) {
  Harness h;
  h.run("init 2");
  auto r = h.run("trade P1 <-> P2 : @#24 <-> @#21");  // nobody owns these
  EXPECT_FALSE(r.ok);
}

TEST(Executor, InvalidCommandReportsError) {
  Harness h;
  auto r = h.run("frobnicate");
  EXPECT_FALSE(r.ok);
}
