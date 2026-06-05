# M11a — Liability Options (Call/Put) & Session Log — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **This is Phase M11a.** Income options (turn-aware, multi-player) are Phase M11b — see `.kiro/specs/options-trading/design.md` "Phase M11b" — and are NOT built here. The data model includes `underlying`/`landers` fields so M11b is additive, but M11a only exercises `Underlying::Liability`.

**Goal:** Let players trade single-roll rent-LIABILITY options — **calls** (`max(U−K,0)`, standard insurance) and **puts** (`max(K−U,0)`, "the insured loses less than K") — bank- or peer-written, strike `K`, escrow-guaranteed; enforce a no-forget settlement gate; add a persistent session log + financial ledger.

**Architecture:** New pure `src/contracts/` module operating on financial state stored in `GameState` (so undo/save-load/rollback cover it for free). Reuses the exact next-roll loss distribution from `pricing::option_chain`. Engine gains `insure`/`write`/`settle`/`log` commands and a `query ledger` view.

**Tech Stack:** C++17, CMake (FetchContent GoogleTest), nlohmann_json, clang/libc++.

**Build/test commands** (used throughout):
- Build: `cmake --build build -j`
- All tests: `ctest --test-dir build --output-on-failure`
- One suite: `ctest --test-dir build -R <Regex> --output-on-failure`

Spec: `.kiro/specs/options-trading/{requirements,design}.md`.

---

## File Structure

- **Create** `src/contracts/contract.h` — `OptionContract`, `LedgerEntry`, `ContractStatus` (POD types; could also live in `domain`, but kept in `contracts` to keep the financial vocabulary together). *Decision: put the POD types in `domain/game_state.h` so `GameState` can hold them without a dependency cycle; `contracts` holds only logic.* See Task 2.
- **Create** `src/contracts/option_book.{h,cpp}` — open/mature/settle/escrow logic.
- **Modify** `src/pricing/option_chain.{h,cpp}` — add `fairPremiumAtStrike`, `maxLossOf`.
- **Modify** `src/domain/game_state.{h,cpp}` — add `OptionContract`/`LedgerEntry` types + `contracts`, `ledger`, `nextContractId` with accessors.
- **Modify** `src/engine/command.h` — `CommandKind::{Insure,Write,Settle,Log}`, `QueryKind::Ledger`, new fields.
- **Modify** `src/engine/parser.cpp` — parse the new verbs.
- **Modify** `src/engine/executor.{h,cpp}` — new cases, maturity hook, settlement gate.
- **Modify** `src/engine/query_service.{h,cpp}` — `ledger` rendering.
- **Modify** `src/engine/help.cpp` — help lines.
- **Modify** `src/engine/session.cpp` — serialize contracts/ledger.
- **Modify** `src/engine/repl.{h,cpp}` — journal accumulation + `log` interception + file append.
- **Modify** `CMakeLists.txt` — add `option_book.cpp` to core; add test files.
- **Create** tests: `test_pricing_strike.cpp`, `test_option_book.cpp`, `test_options_dsl.cpp`, `test_options_exec.cpp`, `test_options_session.cpp`, `test_options_log.cpp`.

---

## Task 1: Pricing helpers — fair premium & max loss at arbitrary strike

**Files:**
- Modify: `src/pricing/option_chain.h`, `src/pricing/option_chain.cpp`
- Test: `tests/unit/test_pricing_strike.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the test file and register it in CMake**

Add `tests/unit/test_pricing_strike.cpp` to the `add_executable(monopoly_tests …)` list in `CMakeLists.txt` (after `tests/unit/test_option_chain.cpp`).

```cpp
// tests/unit/test_pricing_strike.cpp
#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "pricing/insurance_pricer.h"
#include "pricing/option_chain.h"
#include "probability/landing.h"

using namespace monopoly;
using domain::GameState;

namespace {
GameState hotelOnTrafalgar(const domain::Board& board) {
  GameState gs(board);
  gs.addPlayer("P1", 0);
  gs.addPlayer("P2", 0);
  gs.player(0).position = 18;
  gs.setOwner(24, 1); gs.setOwner(21, 1); gs.setOwner(23, 1);  // RED monopoly
  gs.setHouses(24, 5);  // hotel
  return gs;
}
}  // namespace

TEST(PricingStrike, FairPremiumAtZeroEqualsExpectedLoss) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = hotelOnTrafalgar(board);

  auto loss = pricing::buildLossDistribution(gs, 0, resolver);
  auto q = pricing::priceNextRoll(gs, 0, resolver, pricing::PricingConfig{});
  EXPECT_NEAR(pricing::fairPremiumAtStrike(loss, 0.0), q.expectedRent, 1e-6);
}

TEST(PricingStrike, PremiumIsNonIncreasingAndZeroAtMaxLoss) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = hotelOnTrafalgar(board);

  auto loss = pricing::buildLossDistribution(gs, 0, resolver);
  const double maxL = pricing::maxLossOf(loss);
  EXPECT_GT(maxL, 0.0);
  EXPECT_NEAR(pricing::fairPremiumAtStrike(loss, maxL), 0.0, 1e-6);
  EXPECT_LE(pricing::fairPremiumAtStrike(loss, maxL / 2.0),
            pricing::fairPremiumAtStrike(loss, 0.0) + 1e-9);
}

TEST(PricingStrike, EmptyLossYieldsZero) {
  std::vector<pricing::LossOutcome> empty;
  EXPECT_EQ(pricing::fairPremiumAtStrike(empty, 0.0), 0.0);
  EXPECT_EQ(pricing::fairValueAtStrike(empty, 1000.0, /*isPut=*/true), 1000.0);  // P(L=0)=1
  EXPECT_EQ(pricing::maxLossOf(empty), 0.0);
}

TEST(PricingStrike, PutAddsNoLiabilityMassAndParityHolds) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  auto gs = hotelOnTrafalgar(board);
  auto loss = pricing::buildLossDistribution(gs, 0, resolver);

  const double K = pricing::maxLossOf(loss);  // high strike
  const double call = pricing::fairValueAtStrike(loss, K, /*isPut=*/false);
  const double put = pricing::fairValueAtStrike(loss, K, /*isPut=*/true);
  // E[U] = premium at strike 0 (call value at K=0).
  const double EU = pricing::fairValueAtStrike(loss, 0.0, /*isPut=*/false);
  // Put-call parity on the discrete distribution: call - put == E[U] - K.
  EXPECT_NEAR(call - put, EU - K, 1e-3);
  EXPECT_GE(put, 0.0);
}
```

- [ ] **Step 2: Run the test, verify it fails to compile**

Run: `cmake --build build -j 2>&1 | head` — Expected: error, `fairPremiumAtStrike`/`maxLossOf` not declared.

- [ ] **Step 3: Declare the helpers in `option_chain.h`**

Add after the `buildLossDistribution` declaration:

```cpp
// Call fair value E[max(L - K, 0)] for an arbitrary strike K over a loss distribution.
double fairPremiumAtStrike(const std::vector<LossOutcome>& loss, double K);

// Generalized fair value for a call (isPut=false) or put (isPut=true). The loss
// distribution only carries rent>0 outcomes; the put adds the no-liability mass
// P(L=0)=1-Σprob at value 0, paying K there: put = (1-Σprob)*K + Σ max(K-rent,0).
double fairValueAtStrike(const std::vector<LossOutcome>& loss, double K, bool isPut);

// Worst-case single-roll rent over the distribution (0 if empty).
double maxLossOf(const std::vector<LossOutcome>& loss);
```

- [ ] **Step 4: Implement in `option_chain.cpp`**

Add inside `namespace monopoly::pricing { … }`:

```cpp
double fairPremiumAtStrike(const std::vector<LossOutcome>& loss, double K) {
  double premium = 0.0;
  for (const auto& o : loss)
    if (o.rent > K) premium += o.prob * (o.rent - K);
  return premium;
}

double fairValueAtStrike(const std::vector<LossOutcome>& loss, double K, bool isPut) {
  if (!isPut) return fairPremiumAtStrike(loss, K);
  double mass = 0.0, value = 0.0;
  for (const auto& o : loss) {
    mass += o.prob;
    if (o.rent < K) value += o.prob * (K - o.rent);
  }
  value += (1.0 - mass) * K;  // no-liability mass (L == 0) pays K under a put
  return value;
}

double maxLossOf(const std::vector<LossOutcome>& loss) {
  double m = 0.0;
  for (const auto& o : loss) m = std::max(m, o.rent);
  return m;
}
```

(`<algorithm>` is already included for `std::max`.)

- [ ] **Step 5: Build and run the test, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R PricingStrike --output-on-failure`
Expected: 3 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/pricing/option_chain.h src/pricing/option_chain.cpp tests/unit/test_pricing_strike.cpp CMakeLists.txt
git commit -m "feat(pricing): fair premium and max-loss at arbitrary strike"
```

---

## Task 2: Domain data model — contracts & ledger in GameState

**Files:**
- Modify: `src/domain/game_state.h`
- Test: `tests/unit/test_game_state.cpp` (append)

- [ ] **Step 1: Add a test for the new accessors**

Append to `tests/unit/test_game_state.cpp` (it already `#include`s `domain/game_state.h` and uses `monopoly::domain`):

```cpp
TEST(GameStateContracts, StoresAndIdsContracts) {
  using namespace monopoly::domain;
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  gs.addPlayer("P1", 1000); gs.addPlayer("P2", 1000);

  OptionContract c;
  c.id = gs.nextContractId();
  c.writer = 1; c.holder = 0; c.insured = 0; c.strike = 0; c.premium = 100;
  gs.addContract(c);

  ASSERT_EQ(gs.contracts().size(), 1u);
  EXPECT_EQ(gs.contracts()[0].id, 1);
  EXPECT_EQ(gs.nextContractId(), 2);  // monotonic

  LedgerEntry le;
  le.id = 1; le.writer = 1; le.holder = 0; le.insured = 0;
  le.strike = 0; le.premium = 100; le.escrow = 0; le.payout = 50;
  gs.ledger().push_back(le);
  EXPECT_EQ(gs.ledger().size(), 1u);
}
```

> Note: `test_game_state.cpp` must `#include "domain/board_factory.h"` — add it if absent.

- [ ] **Step 2: Run, verify compile failure**

Run: `cmake --build build -j 2>&1 | head` — Expected: `OptionContract`/`addContract`/`nextContractId`/`contracts`/`ledger` undeclared.

- [ ] **Step 3: Add the types and fields to `game_state.h`**

Add above `class GameState` (after the `BankState` struct):

```cpp
enum class ContractStatus { Open, Matured, Settled };
enum class OptionType { Call, Put };           // direction
enum class Underlying { Liability, Income };   // Income == Phase M11b

// A single-roll option. writer == kUnowned (-1) means the bank.
struct OptionContract {
  int id = 0;
  int writer = kUnowned;   // -1 == bank
  int holder = -1;         // the long (premium payer, claim receiver)
  int insured = -1;        // liability: the roller; income (M11b): the owner Py
  OptionType type = OptionType::Call;
  Underlying underlying = Underlying::Liability;
  long strike = 0;         // K
  long premium = 0;        // holder -> writer at open
  long escrow = 0;         // locked from a peer writer (0 for bank)
  ContractStatus status = ContractStatus::Open;
  long realizedValue = 0;  // liability: rent insured paid; income: rent Py collected
  std::vector<int> landers;   // income only (M11b): referenced opponents
  int landersRolled = 0;      // income only (M11b)
};

// One settled contract, appended to the ledger. payout = writer -> holder.
struct LedgerEntry {
  int id = 0, writer = kUnowned, holder = -1, insured = -1;
  OptionType type = OptionType::Call;
  Underlying underlying = Underlying::Liability;
  long strike = 0, premium = 0, escrow = 0, payout = 0;
};
```

Add to the public section of `GameState` (after the Free-Parking accessors):

```cpp
  // Option contracts & settlement ledger --------------------------------
  std::vector<OptionContract>& contracts() { return contracts_; }
  const std::vector<OptionContract>& contracts() const { return contracts_; }
  std::vector<LedgerEntry>& ledger() { return ledger_; }
  const std::vector<LedgerEntry>& ledger() const { return ledger_; }
  int nextContractId() { return nextContractId_; }            // peek next id
  void addContract(OptionContract c) {                        // assigns + bumps id
    if (c.id == 0) c.id = nextContractId_;
    if (c.id >= nextContractId_) nextContractId_ = c.id + 1;
    contracts_.push_back(c);
  }
  void setNextContractId(int n) { nextContractId_ = n; }      // for load
```

Add to the private section:

```cpp
  std::vector<OptionContract> contracts_;
  std::vector<LedgerEntry> ledger_;
  int nextContractId_ = 1;
```

> `<vector>` is already included.

- [ ] **Step 4: Build and run, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R GameStateContracts --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/domain/game_state.h tests/unit/test_game_state.cpp
git commit -m "feat(domain): option contracts and settlement ledger in GameState"
```

---

## Task 3: contracts::OptionBook — open / mature / settle / gate

**Files:**
- Create: `src/contracts/option_book.h`, `src/contracts/option_book.cpp`
- Test: `tests/unit/test_option_book.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the header**

```cpp
// src/contracts/option_book.h
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
// locked from the writer (fails if writer cash < escrow). `quotedFair` returns the
// engine's fair value for the command echo.
OpenResult openPeer(domain::GameState& gs, int writer, int holder, int insured,
                    domain::OptionType type, long strike, long premium,
                    const probability::LandingResolver& resolver);

// Flip every Open liability contract on `insured` to Matured, recording realizedValue.
void matureOnRoll(domain::GameState& gs, int insured, long realizedValue);

// True if any contract is awaiting settlement (the no-forget gate predicate).
bool hasMatured(const domain::GameState& gs);

// Human-readable description of the first matured contract (for the gate message).
std::string firstMaturedSummary(const domain::GameState& gs);

// Settle one matured contract by id, or all of them when allMatured is true.
SettleResult settle(domain::GameState& gs, int id, bool allMatured);

}  // namespace monopoly::contracts
```

- [ ] **Step 2: Write the failing test and register it in CMake**

Add `tests/unit/test_option_book.cpp` to `CMakeLists.txt` test list, and add `src/contracts/option_book.cpp` to the `add_library(monopoly_core …)` list (after `src/pricing/option_chain.cpp`).

```cpp
// tests/unit/test_option_book.cpp
#include <gtest/gtest.h>
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
    gs.addPlayer("P1", 10000000);   // holder/insured
    gs.addPlayer("P2", 10000000);   // peer writer
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

  // Insured paid £3M rent this roll.
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
  EXPECT_GE(f.gs.player(1).cash, wBefore);               // writer never goes negative from settle
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
  // Put with strike £2M: holder profits when realized rent < £2M.
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
```

- [ ] **Step 3: Run, verify it fails to link**

Run: `cmake --build build -j 2>&1 | head` — Expected: undefined references / missing `option_book.cpp`.

- [ ] **Step 4: Implement `option_book.cpp`**

```cpp
// src/contracts/option_book.cpp
#include "contracts/option_book.h"

#include <algorithm>
#include "pricing/option_chain.h"

namespace monopoly::contracts {

namespace {
bool validPlayer(const domain::GameState& gs, int id) {
  return id >= 0 && id < gs.numPlayers();
}
long lround(double d) { return static_cast<long>(d < 0 ? d - 0.5 : d + 0.5); }

// Fair value and peer escrow for a liability option of the given type at strike K.
long fairValueOf(const std::vector<pricing::LossOutcome>& loss, domain::OptionType type,
                 long strike) {
  return lround(pricing::fairValueAtStrike(loss, static_cast<double>(strike),
                                           type == domain::OptionType::Put));
}
long escrowOf(const std::vector<pricing::LossOutcome>& loss, domain::OptionType type,
              long strike) {
  if (type == domain::OptionType::Put) return strike;             // payoff peaks at U=0
  return std::max(0L, lround(pricing::maxLossOf(loss)) - strike);  // call
}
long payoffOf(const domain::OptionContract& c) {
  return (c.type == domain::OptionType::Call) ? std::max(c.realizedValue - c.strike, 0L)
                                              : std::max(c.strike - c.realizedValue, 0L);
}
}  // namespace

OpenResult openBank(domain::GameState& gs, int holder, int insured,
                    domain::OptionType type, long strike,
                    const probability::LandingResolver& resolver) {
  OpenResult r;
  if (!validPlayer(gs, holder) || !validPlayer(gs, insured)) {
    r.ok = false; r.error = "unknown player"; return r;
  }
  if (strike < 0) { r.ok = false; r.error = "strike must be >= 0"; return r; }
  auto loss = pricing::buildLossDistribution(gs, insured, resolver);
  const long premium = fairValueOf(loss, type, strike);

  gs.player(holder).cash -= premium;  // premium to the bank (implicit)
  domain::OptionContract c;
  c.writer = domain::kUnowned; c.holder = holder; c.insured = insured;
  c.type = type; c.underlying = domain::Underlying::Liability;
  c.strike = strike; c.premium = premium; c.escrow = 0;
  c.status = domain::ContractStatus::Open;
  gs.addContract(c);

  r.contractId = gs.contracts().back().id;
  r.premium = premium; r.fairValue = premium;
  return r;
}

OpenResult openPeer(domain::GameState& gs, int writer, int holder, int insured,
                    domain::OptionType type, long strike, long premium,
                    const probability::LandingResolver& resolver) {
  OpenResult r;
  if (!validPlayer(gs, writer) || !validPlayer(gs, holder) || !validPlayer(gs, insured)) {
    r.ok = false; r.error = "unknown player"; return r;
  }
  if (writer == holder) { r.ok = false; r.error = "writer and holder must differ"; return r; }
  if (strike < 0) { r.ok = false; r.error = "strike must be >= 0"; return r; }
  if (premium < 0) { r.ok = false; r.error = "premium must be >= 0"; return r; }

  auto loss = pricing::buildLossDistribution(gs, insured, resolver);
  const long escrow = escrowOf(loss, type, strike);
  if (gs.player(writer).cash < escrow) {
    r.ok = false; r.error = "writer cannot cover escrow of " + std::to_string(escrow);
    return r;
  }

  gs.player(holder).cash -= premium;
  gs.player(writer).cash += premium;
  gs.player(writer).cash -= escrow;  // locked

  domain::OptionContract c;
  c.writer = writer; c.holder = holder; c.insured = insured;
  c.type = type; c.underlying = domain::Underlying::Liability;
  c.strike = strike; c.premium = premium; c.escrow = escrow;
  c.status = domain::ContractStatus::Open;
  gs.addContract(c);

  r.contractId = gs.contracts().back().id;
  r.premium = premium; r.escrow = escrow;
  r.fairValue = fairValueOf(loss, type, strike);
  return r;
}

void matureOnRoll(domain::GameState& gs, int insured, long realizedValue) {
  for (auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Open &&
        c.underlying == domain::Underlying::Liability && c.insured == insured) {
      c.status = domain::ContractStatus::Matured;
      c.realizedValue = realizedValue;
    }
}

bool hasMatured(const domain::GameState& gs) {
  for (const auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Matured) return true;
  return false;
}

std::string firstMaturedSummary(const domain::GameState& gs) {
  for (const auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Matured) {
      const long raw = payoffOf(c);
      const long cap = (c.writer == domain::kUnowned) ? raw : c.escrow;
      const long payout = std::min(raw, cap);
      return "#" + std::to_string(c.id) +
             (c.type == domain::OptionType::Put ? " PUT" : " CALL") + " (P" +
             std::to_string(c.insured + 1) + " value " + std::to_string(c.realizedValue) +
             ", strike " + std::to_string(c.strike) + " -> payout " +
             std::to_string(payout) + ")";
    }
  return "";
}

SettleResult settle(domain::GameState& gs, int id, bool allMatured) {
  SettleResult r;
  bool any = false;
  for (auto& c : gs.contracts()) {
    if (c.status != domain::ContractStatus::Matured) continue;
    if (!allMatured && c.id != id) continue;
    any = true;
    const long raw = payoffOf(c);
    const long cap = (c.writer == domain::kUnowned) ? raw : c.escrow;  // bank uncapped
    const long payout = std::min(raw, cap);

    if (c.writer == domain::kUnowned) {       // bank pays directly
      gs.player(c.holder).cash += payout;
    } else {                                   // peer: release escrow, pay holder
      gs.player(c.holder).cash += payout;
      gs.player(c.writer).cash += (c.escrow - payout);
    }
    domain::LedgerEntry le;
    le.id = c.id; le.writer = c.writer; le.holder = c.holder; le.insured = c.insured;
    le.type = c.type; le.underlying = c.underlying;
    le.strike = c.strike; le.premium = c.premium; le.escrow = c.escrow; le.payout = payout;
    gs.ledger().push_back(le);
    c.status = domain::ContractStatus::Settled;
    r.settledCount += 1;
    r.totalPayout += payout;
  }
  if (!any) { r.ok = false; r.error = "no matured contract to settle"; }
  return r;
}

}  // namespace monopoly::contracts
```

- [ ] **Step 5: Build and run all OptionBook tests, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R OptionBook --output-on-failure`
Expected: all 6 PASS. (`PayoutNeverExceedsEscrow_NoDefault` pins the no-default invariant.)

- [ ] **Step 6: Commit**

```bash
git add src/contracts/option_book.h src/contracts/option_book.cpp tests/unit/test_option_book.cpp CMakeLists.txt
git commit -m "feat(contracts): OptionBook open/mature/settle with escrow no-default invariant"
```

---

## Task 4: DSL parsing — insure / write / settle / log / query ledger

**Files:**
- Modify: `src/engine/command.h`, `src/engine/parser.cpp`
- Test: `tests/unit/test_options_dsl.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Extend `command.h`**

Add to `enum class CommandKind` (extend the existing enumerators):

```cpp
  Insure, Write, Settle, Log,
```

Add `Ledger` to `enum class QueryKind`:

```cpp
enum class QueryKind {
  Risk, Options, Dist, Stationary, Value, State, Board, Chain, Forecast, Simulate, Ledger
};
```

Add fields to `struct Command` (after the trade bundle fields):

```cpp
  // options trading
  int insured = -1;        // contract underlying player (defaults to holder if unset)
  bool isPut = false;      // call (default) vs put; executor maps to domain::OptionType
  long strike = 0;         // option strike K
  long premium = 0;        // peer-written premium
  bool hasStrike = false;
  bool hasPremium = false;
  int contractId = -1;     // settle target (-1 with settleAll)
  bool settleAll = false;
```

(`player` = holder for `insure`; `player` = writer and `player2` = holder for `write`. `count` reused for `log N`. `command.h` is engine-layer and may stay independent of `domain`; the executor converts `isPut` to `domain::OptionType`.)

- [ ] **Step 2: Write the failing parse test and register in CMake**

Add `tests/unit/test_options_dsl.cpp` to the CMake test list.

```cpp
// tests/unit/test_options_dsl.cpp
#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "engine/name_table.h"
#include "engine/parser.h"

using namespace monopoly::engine;

namespace {
NameTable names() {
  auto board = monopoly::domain::loadBoardFromFile(BOARD_JSON_PATH);
  return NameTable(board);
}
}  // namespace

TEST(OptionsDsl, InsureSelfDefaultStrikeZero) {
  auto c = parseLine("insure P1", names());
  EXPECT_EQ(c.kind, CommandKind::Insure);
  EXPECT_EQ(c.player, 0);
  EXPECT_EQ(c.insured, 0);     // defaults to holder
  EXPECT_EQ(c.strike, 0);
}

TEST(OptionsDsl, InsureWithInsuredAndStrike) {
  auto c = parseLine("insure P1 P3 strike 200K", names());
  EXPECT_EQ(c.kind, CommandKind::Insure);
  EXPECT_EQ(c.player, 0);
  EXPECT_EQ(c.insured, 2);
  EXPECT_TRUE(c.hasStrike);
  EXPECT_EQ(c.strike, 2000000);   // 200K in K-units (x10^4)? -> see amount note
}

TEST(OptionsDsl, WritePeerCallWithPremium) {
  auto c = parseLine("write P2 -> P1 P3 strike 100K premium 1.5M", names());
  EXPECT_EQ(c.kind, CommandKind::Write);
  EXPECT_EQ(c.player, 1);    // writer
  EXPECT_EQ(c.player2, 0);   // holder
  EXPECT_EQ(c.insured, 2);
  EXPECT_FALSE(c.isPut);     // default call
  EXPECT_TRUE(c.hasPremium);
}

TEST(OptionsDsl, WritePeerPutAndInsureCall) {
  auto w = parseLine("write P2 -> P1 put strike 200K premium 1M", names());
  EXPECT_EQ(w.kind, CommandKind::Write);
  EXPECT_TRUE(w.isPut);
  EXPECT_EQ(w.insured, 0);   // defaults to holder P1
  auto i = parseLine("insure P1 put strike 200K", names());
  EXPECT_EQ(i.kind, CommandKind::Insure);
  EXPECT_TRUE(i.isPut);
  auto i2 = parseLine("insure P1", names());
  EXPECT_FALSE(i2.isPut);    // default call
}

TEST(OptionsDsl, SettleByIdAndAll) {
  auto a = parseLine("settle 3", names());
  EXPECT_EQ(a.kind, CommandKind::Settle);
  EXPECT_EQ(a.contractId, 3);
  EXPECT_FALSE(a.settleAll);
  auto b = parseLine("settle all", names());
  EXPECT_EQ(b.kind, CommandKind::Settle);
  EXPECT_TRUE(b.settleAll);
}

TEST(OptionsDsl, LogWithAndWithoutCount) {
  EXPECT_EQ(parseLine("log", names()).kind, CommandKind::Log);
  auto c = parseLine("log 20", names());
  EXPECT_EQ(c.kind, CommandKind::Log);
  EXPECT_EQ(c.count, 20);
}

TEST(OptionsDsl, QueryLedger) {
  auto c = parseLine("query ledger", names());
  EXPECT_EQ(c.kind, CommandKind::Query);
  EXPECT_EQ(c.query, QueryKind::Ledger);
}
```

> **Amount note:** money tokens go through `parseAmount`, which returns values in the board's £×10⁴ K-units (so `200K` → `2000000`). Use whatever `parseAmount` returns directly; the assertion above (`2000000`) reflects that convention — verify against `tests/unit/test_engine_parse.cpp` and adjust the literal if the existing tests use a different scale.

- [ ] **Step 3: Run, verify failure**

Run: `cmake --build build -j 2>&1 | head` — Expected: unknown `CommandKind::Insure`, etc., and `parseLine` returns `Invalid` for the new verbs.

- [ ] **Step 4: Add parser cases in `parser.cpp`**

Insert these blocks before the `if (verb == "query")` block (so they're checked with the other verbs):

```cpp
  if (verb == "insure") {
    if (!needPlayer(c.player)) return invalid("insure expects a holder");
    if (cur.eat("put")) c.isPut = true; else cur.eat("call");  // default call
    c.insured = c.player;  // default: self-hedge
    if (!cur.done() && (cur.peek()[0] == 'P' || cur.peek()[0] == 'p')) {
      int ins; if (!needPlayer(ins)) return invalid("insure: bad insured player");
      c.insured = ins;
    }
    if (cur.eat("strike")) {
      auto k = cur.done() ? std::nullopt : parseAmount(cur.take());
      if (!k) return invalid("insure: bad strike");
      c.strike = *k; c.hasStrike = true;
    }
    c.kind = CommandKind::Insure; return c;
  }
  if (verb == "write") {
    if (!needPlayer(c.player)) return invalid("write expects a writer");
    if (!cur.eat("->")) return invalid("write expects '->'");
    if (!needPlayer(c.player2)) return invalid("write expects a holder");
    if (cur.eat("put")) c.isPut = true; else cur.eat("call");  // default call
    c.insured = c.player2;  // default: holder is the insured
    if (!cur.done() && (cur.peek()[0] == 'P' || cur.peek()[0] == 'p')) {
      int ins; if (!needPlayer(ins)) return invalid("write: bad insured player");
      c.insured = ins;
    }
    if (cur.eat("strike")) {
      auto k = cur.done() ? std::nullopt : parseAmount(cur.take());
      if (!k) return invalid("write: bad strike");
      c.strike = *k; c.hasStrike = true;
    }
    if (!cur.eat("premium")) return invalid("write expects 'premium <amount>'");
    auto p = cur.done() ? std::nullopt : parseAmount(cur.take());
    if (!p) return invalid("write: bad premium");
    c.premium = *p; c.hasPremium = true;
    c.kind = CommandKind::Write; return c;
  }
  if (verb == "settle") {
    if (cur.eat("all")) { c.settleAll = true; c.kind = CommandKind::Settle; return c; }
    auto id = cur.done() ? std::nullopt : parseInt(cur.take());
    if (!id) return invalid("settle expects an id or 'all'");
    c.contractId = *id; c.kind = CommandKind::Settle; return c;
  }
  if (verb == "log") {
    if (!cur.done()) { auto n = parseInt(cur.take()); c.count = n ? *n : 0; }
    c.kind = CommandKind::Log; return c;
  }
```

Add inside the `query` block (next to the other subcommands):

```cpp
    if (sub == "ledger") { c.query = QueryKind::Ledger; return c; }
```

> **Note:** `needPlayer` consumes a token. Because both `insure` and `write` peek for an optional insured player by inspecting `cur.peek()[0]`, guard against an empty token: replace the peek condition with `(!cur.done() && parsePlayerPeek(cur.peek()))` if you prefer — but the `[0]` check is safe since `lex` never emits empty tokens. The `int ins` lambda call uses the existing `needPlayer(ins)` helper which reads the next token.

- [ ] **Step 5: Build and run, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsDsl --output-on-failure`
Expected: all PASS. If the `200K` literal assertion fails, fix it to match `parseAmount`'s scale and re-run.

- [ ] **Step 6: Commit**

```bash
git add src/engine/command.h src/engine/parser.cpp tests/unit/test_options_dsl.cpp CMakeLists.txt
git commit -m "feat(engine): parse insure/write/settle/log and query ledger"
```

---

## Task 5: Executor — open/settle cases, maturity hook, settlement gate

**Files:**
- Modify: `src/engine/executor.cpp`, `src/engine/executor.h`
- Test: `tests/unit/test_options_exec.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing executor test and register in CMake**

Add `tests/unit/test_options_exec.cpp` to the CMake test list. (Mirror the construction style in `tests/unit/test_engine_exec.cpp` — read it first to match how an `Executor` is built there, including the `Decks` and `RuleConfig` arguments.)

```cpp
// tests/unit/test_options_exec.cpp
#include <gtest/gtest.h>
#include "contracts/option_book.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "engine/executor.h"
#include "engine/parser.h"
#include "engine/name_table.h"
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
  // Mature it directly to isolate the gate behavior.
  contracts::matureOnRoll(h.gs, 0, 3000000);
  auto blocked = h.run("buy P1 @#1");   // any mutating command
  EXPECT_FALSE(blocked.ok);
  EXPECT_NE(blocked.effects.empty(), true);
  // settle clears the gate, then mutation works again.
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
  // P1 at 18 rolling 6 (=3,3) lands on 24 (Trafalgar hotel) and pays rent.
  h.run("roll P1 = 3,3");
  EXPECT_TRUE(contracts::hasMatured(h.gs));
}
```

> If the existing `test_engine_exec.cpp` constructs `Executor` differently (e.g. different ctor args), match that exactly; the harness above assumes `Executor(gs, decks, rules)`.

- [ ] **Step 2: Run, verify failure**

Run: `cmake --build build -j 2>&1 | head` — Expected: `command not implemented` for `insure`/`write`/`settle`; gate test fails (mutation succeeds).

- [ ] **Step 3: Add the gate + cases in `executor.cpp`**

At the top of `executor.cpp`, add the include:

```cpp
#include "contracts/option_book.h"
```

In `execute(...)`, immediately **after** `snapshot();  // mutating commands from here` (line ~130) and **before** the mutating `switch`, insert the gate:

```cpp
  // No-forget settlement gate: matured contracts must be settled before any
  // further mutating command (settle/undo/save/log/query are routed earlier).
  if (c.kind != CommandKind::Settle && contracts::hasMatured(gs_)) {
    r.fail("settlement required before continuing — settle " +
           contracts::firstMaturedSummary(gs_) + "  (or: settle all)");
    r.prompt("settle all");
    return r;  // snapshot rolled back by the failure path below is not hit; restore here
  }
```

> Because the gate returns before the trailing rollback block, restore the snapshot explicitly: change the gate to pop the snapshot first:
> ```cpp
>   if (c.kind != CommandKind::Settle && contracts::hasMatured(gs_)) {
>     gs_ = history_.back(); history_.pop_back();   // undo the snapshot we just took
>     r.fail("settlement required before continuing — settle " +
>            contracts::firstMaturedSummary(gs_) + "  (or: settle all)");
>     r.prompt("settle all");
>     return r;
>   }
> ```

Add these cases inside the mutating `switch` (before `default:`):

```cpp
    case CommandKind::Insure: {
      if (!validPlayer(c.player, r)) break;
      int insured = (c.insured >= 0) ? c.insured : c.player;
      if (!validPlayer(insured, r)) break;
      const auto type = c.isPut ? domain::OptionType::Put : domain::OptionType::Call;
      auto res = contracts::openBank(gs_, c.player, insured, type, c.strike, resolver_);
      if (!res.ok) { r.fail(res.error); break; }
      r.add(EffectKind::Info, "P" + std::to_string(c.player + 1) + " bought " +
            (c.isPut ? "PUT" : "CALL") + " on P" + std::to_string(insured + 1) +
            " (bank) #" + std::to_string(res.contractId) + " strike " +
            formatMoney(c.strike) + " premium " + formatMoney(res.premium));
      break;
    }
    case CommandKind::Write: {
      if (!validPlayer(c.player, r) || !validPlayer(c.player2, r)) break;
      int insured = (c.insured >= 0) ? c.insured : c.player2;
      if (!validPlayer(insured, r)) break;
      const auto type = c.isPut ? domain::OptionType::Put : domain::OptionType::Call;
      auto res = contracts::openPeer(gs_, c.player, c.player2, insured, type, c.strike,
                                     c.premium, resolver_);
      if (!res.ok) { r.fail(res.error); break; }
      r.add(EffectKind::Info, "P" + std::to_string(c.player + 1) + " wrote " +
            (c.isPut ? "PUT" : "CALL") + " to P" + std::to_string(c.player2 + 1) +
            " on P" + std::to_string(insured + 1) + " #" + std::to_string(res.contractId) +
            " premium " + formatMoney(res.premium) + " (fair " +
            formatMoney(res.fairValue) + ") escrow " + formatMoney(res.escrow));
      break;
    }
    case CommandKind::Settle: {
      auto res = contracts::settle(gs_, c.contractId, c.settleAll);
      if (!res.ok) { r.fail(res.error); break; }
      r.add(EffectKind::CashTransfer, "settled " + std::to_string(res.settledCount) +
            " contract(s), total payout " + formatMoney(res.totalPayout));
      break;
    }
```

> **`query_.resolver()`** — the `QueryService` already owns a `LandingResolver` (it renders `query chain`). Expose it: add `const probability::LandingResolver& resolver() const { return resolver_; }` to `query_service.h` if not already public. If `QueryService` builds the resolver lazily, instead store a `LandingResolver` member in `Executor` built from `decks` in the ctor and pass that. **Decision:** add a `LandingResolver resolver_;` member to `Executor`, built in the ctor from the board + decks, and use `resolver_` in the cases above. Update `executor.h` accordingly (add `#include "probability/landing.h"` and the member; initialize in the ctor init list: `resolver_(gs.board(), decks)`).

- [ ] **Step 4: Add the maturity hook in `resolveLanding`**

In `resolveLanding`, the rent branch currently does:

```cpp
      long rent = risk::rentOwed(gs_, pos, player, arrivalSum);
      gs_.player(player).cash -= rent;
      gs_.player(owner).cash += rent;
      r.add(EffectKind::RentPaid, …);
```

Maturity must reflect the rent actually paid this roll, including a £0 result when no rent is owed. At the **end** of `resolveLanding`, after all branches, add:

```cpp
  // Mature any single-roll option on this player against the rent paid this roll.
  long paidRent = 0;
  if (domain::isPurchasable(sq.type)) {
    const int owner = gs_.ownerOf(pos);
    if (owner != domain::kUnowned && owner != player && !gs_.isMortgaged(pos))
      paidRent = risk::rentOwed(gs_, pos, player, arrivalSum);
  }
  if (contracts::hasOpenOn(gs_, player)) {
    contracts::matureOnRoll(gs_, player, paidRent);
    r.add(EffectKind::Info, "option(s) on P" + std::to_string(player + 1) +
          " matured — settle before continuing");
    r.prompt("settle all");
  }
```

Add the small helper `hasOpenOn` to `option_book.{h,cpp}`:

```cpp
// option_book.h
bool hasOpenOn(const domain::GameState& gs, int insured);
```
```cpp
// option_book.cpp
bool hasOpenOn(const domain::GameState& gs, int insured) {
  for (const auto& c : gs.contracts())
    if (c.status == domain::ContractStatus::Open && c.insured == insured) return true;
  return false;
}
```

> **Avoid double-counting rent:** `resolveLanding` already deducted `rent` from the player in the rent branch. The maturity block **re-computes** `paidRent` for the matured value but must **not** deduct again — the code above only reads it. Confirm by inspection that no second deduction occurs.

- [ ] **Step 5: Build and run, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsExec --output-on-failure`
Expected: all PASS.

- [ ] **Step 6: Run the full suite to catch regressions**

Run: `ctest --test-dir build --output-on-failure`
Expected: all green (existing tests unaffected).

- [ ] **Step 7: Commit**

```bash
git add src/engine/executor.cpp src/engine/executor.h src/contracts/option_book.h src/contracts/option_book.cpp tests/unit/test_options_exec.cpp CMakeLists.txt
git commit -m "feat(engine): insure/write/settle commands, roll maturity hook, no-forget gate"
```

---

## Task 6: query ledger rendering + help

**Files:**
- Modify: `src/engine/query_service.cpp`, `src/engine/query_service.h`, `src/engine/help.cpp`
- Test: `tests/unit/test_options_exec.cpp` (append)

- [ ] **Step 1: Append a ledger-rendering test**

```cpp
TEST(OptionsExec, QueryLedgerListsSettledAndOpen) {
  Harness h;
  ASSERT_TRUE(h.run("insure P1 strike 0").ok);
  auto before = h.run("query ledger");
  EXPECT_TRUE(before.ok);
  ASSERT_FALSE(before.effects.empty());
  EXPECT_NE(before.effects.front().text.find("P1"), std::string::npos);  // open row shown
}
```

- [ ] **Step 2: Run, verify failure**

Run: `cmake --build build -j 2>&1 | head` — Expected: `QueryKind::Ledger` unhandled (empty/garbage output or compile error in `query_service`).

- [ ] **Step 3: Handle `Ledger` in `query_service.cpp`**

In `QueryService::handle`, add a `case QueryKind::Ledger:` that builds a string and returns it as a `Query` effect. Match the existing rendering style (look at how `Chain` is rendered):

```cpp
    case QueryKind::Ledger: {
      std::ostringstream os;
      os << "Option ledger\n";
      os << "  open contracts:\n";
      for (const auto& c : gs_.contracts()) {
        if (c.status == domain::ContractStatus::Settled) continue;
        os << "    #" << c.id << "  P" << (c.holder + 1) << " <- "
           << (c.writer == domain::kUnowned ? "BANK" : "P" + std::to_string(c.writer + 1))
           << "  insured P" << (c.insured + 1)
           << "  K=" << formatMoney(c.strike) << "  prem=" << formatMoney(c.premium)
           << "  escrow=" << formatMoney(c.escrow)
           << (c.status == domain::ContractStatus::Matured ? "  [MATURED]" : "") << "\n";
      }
      os << "  settled:\n";
      // net P&L per player from the ledger
      std::vector<long> net(static_cast<std::size_t>(gs_.numPlayers()), 0);
      for (const auto& e : gs_.ledger()) {
        os << "    #" << e.id << "  payout " << formatMoney(e.payout)
           << " (prem " << formatMoney(e.premium) << ")\n";
        net[static_cast<std::size_t>(e.holder)] += e.payout - e.premium;
        if (e.writer != domain::kUnowned)
          net[static_cast<std::size_t>(e.writer)] += e.premium - e.payout;
      }
      os << "  net P&L:";
      for (int p = 0; p < gs_.numPlayers(); ++p)
        os << "  P" << (p + 1) << " " << formatMoney(net[static_cast<std::size_t>(p)]);
      os << "\n";
      CommandResult r;
      r.add(EffectKind::Query, os.str());
      return r;
    }
```

> Ensure `query_service.cpp` includes `<sstream>` and `engine/amount.h` (for `formatMoney`); add if missing. `gs_` is the `QueryService`'s GameState reference — confirm the member name (it may be `state_` or `gs_`; match the file).

- [ ] **Step 4: Add help lines in `help.cpp`**

Add lines documenting the new verbs near the other command help (match existing formatting):

```
  insure P{h} [P{ins}] [strike K]        buy fair-priced next-roll insurance from the bank
  write P{w} -> P{h} [P{ins}] strike K premium P   peer-write an option (escrow locked)
  settle <id> | settle all               settle matured option(s)
  log [N]                                show the session command/effect journal
  query ledger                           option premiums, escrow, payouts, net P&L
```

- [ ] **Step 5: Build and run, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsExec --output-on-failure`
Expected: all PASS.

- [ ] **Step 6: Commit**

```bash
git add src/engine/query_service.cpp src/engine/query_service.h src/engine/help.cpp tests/unit/test_options_exec.cpp
git commit -m "feat(engine): query ledger view and help for options commands"
```

---

## Task 7: Session log — journal in Repl + file persistence

**Files:**
- Modify: `src/engine/repl.h`, `src/engine/repl.cpp`
- Test: `tests/unit/test_options_log.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test and register in CMake**

Add `tests/unit/test_options_log.cpp` to the CMake list. (Look at `tests/unit/test_engine_repl.cpp` for how `Repl` is driven with `std::istringstream`/`std::ostringstream`.)

```cpp
// tests/unit/test_options_log.cpp
#include <gtest/gtest.h>
#include <sstream>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "engine/repl.h"
#include "rules/rule_config.h"

using namespace monopoly;

TEST(OptionsLog, LogReplaysCommandJournal) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  rules::RuleConfig rules;
  std::ostringstream out;
  engine::Repl repl(board, decks, rules, out);

  std::istringstream in("init 2\nbuy P1 @#1\nlog\nquit\n");
  repl.run(in);

  const std::string s = out.str();
  // The log output should echo earlier commands back.
  EXPECT_NE(s.find("init 2"), std::string::npos);
  EXPECT_NE(s.find("buy P1"), std::string::npos);
}
```

- [ ] **Step 2: Run, verify failure**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsLog --output-on-failure`
Expected: FAIL (`log` not echoed; currently unimplemented in repl).

- [ ] **Step 3: Add the journal to `repl.h`**

Add to the `Repl` private section:

```cpp
  struct JournalEntry { int seq; std::string command; std::vector<std::string> effects; };
  std::vector<JournalEntry> journal_;
  int seq_ = 0;
  std::string logPath_ = "monopoly-session.log";
```

Add `#include <string>` / `#include <vector>` if not present.

- [ ] **Step 4: Record + intercept `log` in `repl.cpp`**

In `Repl::run`, after parsing each line and **before** dispatching to the executor, intercept `log`:

```cpp
    Command cmd = parseLine(line, names_);
    if (cmd.kind == CommandKind::Log) {
      const int n = cmd.count;
      const int start = (n > 0 && n < static_cast<int>(journal_.size()))
                        ? static_cast<int>(journal_.size()) - n : 0;
      for (int i = start; i < static_cast<int>(journal_.size()); ++i) {
        out_ << "[" << journal_[static_cast<std::size_t>(i)].seq << "] "
             << journal_[static_cast<std::size_t>(i)].command << "\n";
        for (const auto& e : journal_[static_cast<std::size_t>(i)].effects)
          out_ << "    " << e << "\n";
      }
      continue;  // log is not a game command
    }
    CommandResult result = exec_.execute(cmd);
```

After a command executes (where `render(...)` is called), record it to the journal and append to the file:

```cpp
    // Journal every executed command + its effect texts.
    JournalEntry je;
    je.seq = ++seq_;
    je.command = line;
    for (const auto& e : result.effects) je.effects.push_back(e.text);
    journal_.push_back(je);
    {
      std::ofstream f(logPath_, std::ios::app);
      if (f) {
        f << "[" << je.seq << "] " << je.command << "\n";
        for (const auto& t : je.effects) f << "    " << t << "\n";
      }
    }
```

Add `#include <fstream>` to `repl.cpp`.

> **Scope note:** the file is append-only across runs by design (R6.2). If a test or user wants a clean file, they delete it; we do not truncate. Tests above only assert in-memory `log` output, so the file write is incidental there.

- [ ] **Step 5: Build and run, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsLog --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/engine/repl.h src/engine/repl.cpp tests/unit/test_options_log.cpp CMakeLists.txt
git commit -m "feat(engine): session command/effect journal with log command and file append"
```

---

## Task 8: Save/load round-trip for contracts & ledger

**Files:**
- Modify: `src/engine/session.cpp`
- Test: `tests/unit/test_options_session.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing round-trip test and register in CMake**

Add `tests/unit/test_options_session.cpp` to the CMake list. (Look at how `session.cpp` reads/writes the JSON and reuse the existing save/load entry points.)

```cpp
// tests/unit/test_options_session.cpp
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
  gs.addPlayer("P1", 1000); gs.addPlayer("P2", 1000);
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
```

- [ ] **Step 2: Run, verify failure**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsSession --output-on-failure`
Expected: FAIL (contracts/ledger empty after load).

- [ ] **Step 3: Serialize contracts & ledger in `session.cpp`**

In the save function, after the existing fields, add JSON arrays:

```cpp
  nlohmann::json contracts = nlohmann::json::array();
  for (const auto& c : gs.contracts())
    contracts.push_back({{"id", c.id}, {"writer", c.writer}, {"holder", c.holder},
                         {"insured", c.insured}, {"type", static_cast<int>(c.type)},
                         {"underlying", static_cast<int>(c.underlying)},
                         {"strike", c.strike}, {"premium", c.premium},
                         {"escrow", c.escrow}, {"status", static_cast<int>(c.status)},
                         {"realizedValue", c.realizedValue}, {"landers", c.landers},
                         {"landersRolled", c.landersRolled}});
  j["contracts"] = contracts;
  j["nextContractId"] = gs.nextContractId();

  nlohmann::json ledger = nlohmann::json::array();
  for (const auto& e : gs.ledger())
    ledger.push_back({{"id", e.id}, {"writer", e.writer}, {"holder", e.holder},
                      {"insured", e.insured}, {"type", static_cast<int>(e.type)},
                      {"underlying", static_cast<int>(e.underlying)},
                      {"strike", e.strike}, {"premium", e.premium},
                      {"escrow", e.escrow}, {"payout", e.payout}});
  j["ledger"] = ledger;
```

In the load function, after the existing fields, rebuild them (guard with `contains` for backward compatibility with old saves):

```cpp
  if (j.contains("contracts")) {
    for (const auto& jc : j["contracts"]) {
      domain::OptionContract c;
      c.id = jc.at("id"); c.writer = jc.at("writer"); c.holder = jc.at("holder");
      c.insured = jc.at("insured");
      c.type = static_cast<domain::OptionType>(jc.at("type").get<int>());
      c.underlying = static_cast<domain::Underlying>(jc.at("underlying").get<int>());
      c.strike = jc.at("strike"); c.premium = jc.at("premium"); c.escrow = jc.at("escrow");
      c.status = static_cast<domain::ContractStatus>(jc.at("status").get<int>());
      c.realizedValue = jc.at("realizedValue");
      c.landers = jc.at("landers").get<std::vector<int>>();
      c.landersRolled = jc.at("landersRolled");
      gs.addContract(c);
    }
  }
  if (j.contains("nextContractId")) gs.setNextContractId(j["nextContractId"].get<int>());
  if (j.contains("ledger")) {
    for (const auto& je : j["ledger"]) {
      domain::LedgerEntry e;
      e.id = je.at("id"); e.writer = je.at("writer"); e.holder = je.at("holder");
      e.insured = je.at("insured");
      e.type = static_cast<domain::OptionType>(je.at("type").get<int>());
      e.underlying = static_cast<domain::Underlying>(je.at("underlying").get<int>());
      e.strike = je.at("strike"); e.premium = je.at("premium");
      e.escrow = je.at("escrow"); e.payout = je.at("payout");
      gs.ledger().push_back(e);
    }
  }
```

> Match the actual JSON variable name in `session.cpp` (it may be `j`, `root`, etc.). `loadGame` rebuilds `gs` in place — ensure contracts are added *after* players exist, and that a freshly constructed `gs2` starts empty (it does).

- [ ] **Step 4: Build and run, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsSession --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/engine/session.cpp tests/unit/test_options_session.cpp CMakeLists.txt
git commit -m "feat(engine): persist option contracts and ledger across save/load"
```

---

## Task 9: Integration — undo atomicity + full-suite green + docs

**Files:**
- Test: `tests/unit/test_options_exec.cpp` (append)
- Modify: `.kiro/specs/markov-advisor/tasks.md` (roadmap row), `README` if it lists commands

- [ ] **Step 1: Append an undo-atomicity test**

```cpp
TEST(OptionsExec, UndoReversesOpenAndMaturityAndSettle) {
  Harness h;
  long cash0 = h.gs.player(0).cash;
  ASSERT_TRUE(h.run("insure P1 strike 0").ok);
  EXPECT_LT(h.gs.player(0).cash, cash0);     // premium left
  ASSERT_TRUE(h.run("undo").ok);
  EXPECT_EQ(h.gs.player(0).cash, cash0);     // premium restored
  EXPECT_TRUE(h.gs.contracts().empty());     // contract gone
}
```

- [ ] **Step 2: Run it, verify pass**

Run: `cmake --build build -j && ctest --test-dir build -R OptionsExec --output-on-failure`
Expected: PASS (undo works for free because contracts live in `GameState`).

- [ ] **Step 3: Run the full suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: ALL green.

- [ ] **Step 4: Update the roadmap**

Add a row to `.kiro/specs/markov-advisor/tasks.md` milestone table:

```
| **M11** | **Options trading & log** | ✅ done | tradeable single-roll options (bank + peer, strike K, escrow no-default), no-forget settlement gate, `query ledger`, session `log` + file persistence | `.kiro/specs/options-trading/plans/milestone-11-options-trading.md` | M4, M8 |
```

- [ ] **Step 5: Commit**

```bash
git add tests/unit/test_options_exec.cpp .kiro/specs/markov-advisor/tasks.md
git commit -m "test(options): undo atomicity; docs: roadmap M11"
```

---

## Self-Review (completed by plan author)

**Spec coverage:** R1 (contract model) → Task 2; R1.4 payout cap → Task 3 + `PayoutNeverExceedsEscrow`. R2 bank/peer → Task 3 (`openBank`/`openPeer`) + Task 5 cases. R3 escrow/no-default → Task 3. R4 lifecycle + gate → Task 5. R5 undo/save-load → Task 8 + Task 9 undo test. R6 log/ledger → Tasks 6 & 7. R7 tests → distributed across Tasks 1–9.

**Placeholder scan:** No TBD/TODO; every code step has concrete code. Two steps explicitly flag *verify-against-existing-file* points (Executor ctor signature in Task 5; `parseAmount` scale in Task 4; JSON var name in Task 8) — these are real integration checks, not placeholders, and each says exactly what to confirm and how.

**Type consistency:** `OptionContract`/`LedgerEntry`/`ContractStatus` defined in Task 2 and used verbatim in Tasks 3/5/6/8. `OpenResult`/`SettleResult` defined in Task 3 header and consumed in Task 5. `fairPremiumAtStrike`/`maxLossOf` defined Task 1, used Task 3. `hasOpenOn` added in Task 5 alongside its use. Command fields (`insured`, `strike`, `premium`, `contractId`, `settleAll`, `count`) defined Task 4, used Task 5.

**Known integration risks to watch during execution (not blockers):**
1. `Executor` ctor arg order — match `test_engine_exec.cpp`.
2. `QueryService` GameState member name (`gs_` vs `state_`) and resolver exposure — Task 5 chooses to add a `resolver_` to `Executor` to avoid coupling.
3. `parseAmount` unit scale for the strike literal assertion in Task 4.
