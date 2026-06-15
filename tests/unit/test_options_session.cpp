#include <gtest/gtest.h>
#include <cstdio>
#include "domain/board_factory.h"
#include "domain/game_state.h"
#include "engine/session.h"
#include "rules/rule_config.h"

using namespace monopoly;

TEST(OptionsSession, ContractsAndLedgerRoundTrip) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  domain::GameState gs(board);
  gs.addPlayer("P1", 1000);
  gs.addPlayer("P2", 1000);
  domain::OptionContract c;
  c.id = gs.nextContractId(); c.writer = 1; c.holder = 0; c.insured = 0;
  c.type = domain::OptionType::Put; c.underlying = domain::Underlying::Liability;
  c.strike = 100; c.premium = 50; c.escrow = 900;
  c.status = domain::ContractStatus::Matured; c.realizedValue = 300;
  gs.addContract(c);
  domain::LedgerEntry le;
  le.id = 1; le.writer = 1; le.holder = 0; le.insured = 0;
  le.type = domain::OptionType::Put; le.strike = 100; le.premium = 50;
  le.escrow = 900; le.payout = 200;
  gs.ledger().push_back(le);

  rules::RuleConfig rules; int nextRoller = 1;
  const std::string path = "test_options_session.json";
  engine::saveGame(gs, rules, nextRoller, path);

  domain::GameState gs2(board);
  rules::RuleConfig rules2; int nr2 = 0;
  engine::loadGame(gs2, rules2, nr2, path);
  std::remove(path.c_str());

  ASSERT_EQ(gs2.contracts().size(), 1u);
  EXPECT_EQ(gs2.contracts()[0].escrow, 900);
  EXPECT_EQ(gs2.contracts()[0].status, domain::ContractStatus::Matured);
  EXPECT_EQ(gs2.contracts()[0].type, domain::OptionType::Put);
  EXPECT_EQ(gs2.contracts()[0].realizedValue, 300);
  ASSERT_EQ(gs2.ledger().size(), 1u);
  EXPECT_EQ(gs2.ledger()[0].payout, 200);
  EXPECT_EQ(gs2.nextContractId(), 2);
}
