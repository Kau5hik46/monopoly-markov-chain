#include <gtest/gtest.h>
#include <algorithm>
#include "contracts/option_book.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "probability/landing.h"

using namespace monopoly;
using domain::GameState;
using domain::ContractStatus;
using domain::OptionType;

namespace {
struct Fixture {
  domain::Board board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  domain::Decks decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver{board, decks};
  GameState gs{board};
  Fixture() {
    gs.addPlayer("P1", 50000000);   // holder/insured (>= max loss so escrow is coverable)
    gs.addPlayer("P2", 50000000);   // peer writer
    gs.player(0).position = 18;
    gs.setOwner(24, 1); gs.setOwner(21, 1); gs.setOwner(23, 1);  // RED monopoly to P2
    gs.setHouses(24, 5);            // hotel on Trafalgar
  }
};
}  // namespace

TEST(OptionBook, BankOpenChargesFairPremiumAndNoEscrow) {
  Fixture f;
  long holderBefore = f.gs.player(0).cash;
  auto r = contracts::openBank(f.gs, /*holder=*/0, /*insured=*/0, OptionType::Call,
                               /*K=*/0, f.resolver);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_GT(r.premium, 0);
  EXPECT_EQ(r.escrow, 0);
  EXPECT_EQ(f.gs.player(0).cash, holderBefore - r.premium);  // premium left holder
  ASSERT_EQ(f.gs.contracts().size(), 1u);
  EXPECT_EQ(f.gs.contracts()[0].writer, domain::kUnowned);   // bank
}

TEST(OptionBook, PeerOpenLocksEscrowAndMovesPremium) {
  Fixture f;
  long hBefore = f.gs.player(0).cash, wBefore = f.gs.player(1).cash;
  auto r = contracts::openPeer(f.gs, /*writer=*/1, /*holder=*/0, /*insured=*/0,
                               OptionType::Call, /*K=*/0, /*premium=*/500000, f.resolver);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_GT(r.escrow, 0);
  EXPECT_EQ(f.gs.player(0).cash, hBefore - 500000);                 // holder pays premium
  EXPECT_EQ(f.gs.player(1).cash, wBefore + 500000 - r.escrow);      // writer +prem -escrow
}

TEST(OptionBook, PeerOpenFailsWhenWriterCannotCoverEscrow) {
  Fixture f;
  f.gs.player(1).cash = 100;  // far below maxLoss
  auto r = contracts::openPeer(f.gs, 1, 0, 0, OptionType::Call, 0, 50, f.resolver);
  EXPECT_FALSE(r.ok);
  EXPECT_TRUE(f.gs.contracts().empty());
}

TEST(OptionBook, MatureThenSettlePaysCappedAtEscrowAndReleasesRest) {
  Fixture f;
  auto open = contracts::openPeer(f.gs, 1, 0, 0, OptionType::Call, /*K=*/0,
                                  /*premium=*/500000, f.resolver);
  ASSERT_TRUE(open.ok);
  long hAfterOpen = f.gs.player(0).cash, wAfterOpen = f.gs.player(1).cash;

  contracts::matureOnRoll(f.gs, /*insured=*/0, /*realizedValue=*/3000000);
  EXPECT_TRUE(contracts::hasMatured(f.gs));
  EXPECT_EQ(f.gs.contracts()[0].status, ContractStatus::Matured);

  auto s = contracts::settle(f.gs, /*id=*/0, /*all=*/true);
  ASSERT_TRUE(s.ok) << s.error;
  EXPECT_FALSE(contracts::hasMatured(f.gs));
  long payout = 3000000;  // min(max(3M-0,0), escrow); escrow >= 3M for a hotel
  EXPECT_EQ(f.gs.player(0).cash, hAfterOpen + payout);              // holder receives
  EXPECT_EQ(f.gs.player(1).cash, wAfterOpen + (open.escrow - payout));  // writer releases rest
  ASSERT_EQ(f.gs.ledger().size(), 1u);
  EXPECT_EQ(f.gs.ledger()[0].payout, payout);
}

TEST(OptionBook, PayoutNeverExceedsEscrow_NoDefault) {
  Fixture f;
  auto open = contracts::openPeer(f.gs, 1, 0, 0, OptionType::Call, 0, 1, f.resolver);
  ASSERT_TRUE(open.ok);
  contracts::matureOnRoll(f.gs, 0, /*absurd rent=*/999999999);  // > escrow
  long wBefore = f.gs.player(1).cash;
  auto s = contracts::settle(f.gs, 0, true);
  ASSERT_TRUE(s.ok);
  EXPECT_LE(s.totalPayout, open.escrow);                 // capped
  EXPECT_GE(f.gs.player(1).cash, wBefore);               // writer never negative from settle
}

TEST(OptionBook, ZeroRentMaturityPaysNothingButMustStillSettle) {
  Fixture f;
  auto open = contracts::openPeer(f.gs, 1, 0, 0, OptionType::Call, 0, 200000, f.resolver);
  ASSERT_TRUE(open.ok);
  long wAfterOpen = f.gs.player(1).cash;
  contracts::matureOnRoll(f.gs, 0, /*realizedValue=*/0);
  EXPECT_TRUE(contracts::hasMatured(f.gs));
  auto s = contracts::settle(f.gs, 0, true);
  ASSERT_TRUE(s.ok);
  EXPECT_EQ(s.totalPayout, 0);
  EXPECT_EQ(f.gs.player(1).cash, wAfterOpen + open.escrow);  // full escrow released
}

TEST(OptionBook, PutPaysWhenRentStaysBelowStrike) {
  Fixture f;
  auto open = contracts::openPeer(f.gs, /*writer=*/1, /*holder=*/0, /*insured=*/0,
                                  OptionType::Put, /*K=*/2000000, /*premium=*/300000,
                                  f.resolver);
  ASSERT_TRUE(open.ok) << open.error;
  EXPECT_EQ(open.escrow, 2000000);  // put escrow == strike (payoff peaks at U=0)
  long hAfterOpen = f.gs.player(0).cash;
  contracts::matureOnRoll(f.gs, 0, /*realizedValue=*/500000);  // low rent
  auto s = contracts::settle(f.gs, 0, true);
  ASSERT_TRUE(s.ok);
  EXPECT_EQ(s.totalPayout, 1500000);                  // max(2M - 0.5M, 0)
  EXPECT_EQ(f.gs.player(0).cash, hAfterOpen + 1500000);
}

TEST(OptionBook, IncomePeerOpenLocksEscrow) {
  Fixture f;
  auto r = contracts::openIncomePeer(f.gs, /*writer=*/0, /*holder=*/1, /*owner=*/1,
                                     OptionType::Call, /*K=*/0, /*premium=*/200000,
                                     {0}, f.resolver);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_GT(r.escrow, 0);
  EXPECT_GT(r.fairValue, 0);
  ASSERT_EQ(f.gs.contracts().size(), 1u);
  EXPECT_EQ(f.gs.contracts()[0].underlying, domain::Underlying::Income);
}

TEST(OptionBook, IncomeAccumulatesThenMaturesAndSettles) {
  Fixture f;
  auto open = contracts::openIncomePeer(f.gs, 0, 1, 1, OptionType::Call, 0, 200000,
                                        {0}, f.resolver);
  ASSERT_TRUE(open.ok);
  long holderBefore = f.gs.player(1).cash;            // P2 holds
  contracts::accumulateIncome(f.gs, /*owner=*/1, /*payer=*/0, 3000000);
  EXPECT_FALSE(contracts::hasMatured(f.gs));           // not until owner's turn
  contracts::matureIncomeOnOwnerTurn(f.gs, 1);
  EXPECT_TRUE(contracts::hasMatured(f.gs));
  auto s = contracts::settle(f.gs, 0, true);
  ASSERT_TRUE(s.ok);
  long payout = std::min(3000000L, open.escrow);       // call payoff capped at escrow
  EXPECT_EQ(s.totalPayout, payout);
  EXPECT_EQ(f.gs.player(1).cash, holderBefore + payout);
}

TEST(OptionBook, IncomeNoDefaultPayoutCappedAtEscrow) {
  Fixture f;
  auto open = contracts::openIncomePeer(f.gs, 0, 1, 1, OptionType::Call, 0, 1,
                                        {0}, f.resolver);
  ASSERT_TRUE(open.ok);
  contracts::accumulateIncome(f.gs, 1, 0, 999999999);
  contracts::matureIncomeOnOwnerTurn(f.gs, 1);
  auto s = contracts::settle(f.gs, 0, true);
  ASSERT_TRUE(s.ok);
  EXPECT_LE(s.totalPayout, open.escrow);
}

TEST(OptionBook, IncomePutPaysWhenIncomeBelowStrike) {
  Fixture f;
  auto open = contracts::openIncomePeer(f.gs, 0, 1, 1, OptionType::Put,
                                        /*K=*/5000000, /*premium=*/100000, {0}, f.resolver);
  ASSERT_TRUE(open.ok);
  EXPECT_EQ(open.escrow, 5000000);                     // put escrow = strike
  long holderBefore = f.gs.player(1).cash;
  contracts::accumulateIncome(f.gs, 1, 0, 1000000);    // low income
  contracts::matureIncomeOnOwnerTurn(f.gs, 1);
  auto s = contracts::settle(f.gs, 0, true);
  ASSERT_TRUE(s.ok);
  EXPECT_EQ(s.totalPayout, 4000000);                   // max(5M - 1M, 0)
  EXPECT_EQ(f.gs.player(1).cash, holderBefore + 4000000);
}
