# Milestone 4: Risk & Pricing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Compute, for the player about to roll, the **expected single-roll rent liability** and a **fair insurance/option premium** for the next roll (requirement A), including a closed-form mugging expected value when that rule is enabled.

**Architecture:** `monopoly::risk` (rent model + rent table + risk indices) and `monopoly::pricing` (insurance pricer / option chain). The pricer enumerates the 36 dice outcomes from the roller's position, resolves card branching via the existing `LandingResolver`, and sums opponent-owned rent (handling the utility dice-dependence exactly because the arrival sum is known per outcome).

**Tech Stack:** C++17, `monopoly::domain::GameState`, `monopoly::probability::LandingResolver`, `monopoly::probability::dice`, GoogleTest.

> **Rent-data note:** the themed board is a re-skin of the standard UK Monopoly board
> (prices are exactly the canonical values ×10,000). So we use the **real UK Monopoly
> rents ×10,000**, stored per-street in `data/board.london.json` and loaded into
> `Square.rent`. Source: jdawiseman.com rent table (verified against the canonical card).

---

## Rent model (real UK Monopoly values ×10,000)

- **Streets:** full per-property schedule `[site, 1h, 2h, 3h, 4h, hotel]` lives in the
  board JSON (`Square.rent`). Undeveloped **monopoly** (whole group owned, no houses) =
  `2 × site`. Mortgaged = `0`.
- **Stations** by count owned: `{1: 250000, 2: 500000, 3: 1000000, 4: 2000000}`
  (£25/50/100/200 ×10⁴).
- **Utilities:** `arrivalSum × (one utility ? 40000 : 100000)` (4×/10× dice ×10⁴).

---

### Task 0: Real rent schedule in board data

**Files:** Modify `src/domain/square.h` (add `rent` field), `src/domain/board_factory.cpp` (parse it), `data/board.london.json` (add real ×10⁴ rent arrays to all 22 streets). Add a test to `tests/unit/test_board.cpp`.

- [ ] **Step 1:** Add to `Square` (in `square.h`) a `std::array<long,6> rent{}` member
  ([site,1h,2h,3h,4h,hotel]; zeros for non-streets). Include `<array>`.
- [ ] **Step 2:** In `board_factory.cpp`, parse an optional `"rent"` array:
  `if (e.contains("rent")) { auto r = e.at("rent"); for (size_t k=0;k<6 && k<r.size();++k) s.rent[k]=r[k].get<long>(); }`
- [ ] **Step 3:** Add `"rent":[site,1h,2h,3h,4h,hotel]` (real UK values ×10000) to each
  street in `data/board.london.json`. Values by position (×10000):
  1:[2,10,30,90,160,250] 3:[4,20,60,180,320,450] 6:[6,30,90,270,400,550]
  8:[6,30,90,270,400,550] 9:[8,40,100,300,450,600] 11:[10,50,150,450,625,750]
  13:[10,50,150,450,625,750] 14:[12,60,180,500,700,900] 16:[14,70,200,550,750,950]
  18:[14,70,200,550,750,950] 19:[16,80,220,600,800,1000] 21:[18,90,250,700,875,1050]
  23:[18,90,250,700,875,1050] 24:[20,100,300,750,925,1100] 26:[22,110,330,800,975,1150]
  27:[22,110,330,800,975,1150] 29:[24,120,360,850,1025,1200] 31:[26,130,390,900,1100,1275]
  32:[26,130,390,900,1100,1275] 34:[28,150,450,1000,1200,1400] 37:[35,175,500,1100,1300,1500]
  39:[50,200,600,1400,1700,2000] — each entry multiplied by 10000.
- [ ] **Step 4:** Test in `test_board.cpp`: `board.at(39).rent[5] == 20000000` (Mayfair-analog hotel), `board.at(1).rent[0] == 20000` (Old-Kent-analog site).
- [ ] **Step 5:** Build + `ctest -R Board` → PASS. **Commit** `feat(domain): real UK Monopoly rent schedules (x10000) in board data`.

### Task 1: rent_model.h + RentTable

**Files:** Create `src/risk/rent_model.h`, `src/risk/rent_table.h` / `rent_table.cpp`, `tests/unit/test_rent.cpp`; modify `CMakeLists.txt`.

- [ ] **Step 1: `src/risk/rent_model.h`** (station/utility rules; streets read their schedule)

```cpp
#pragma once

namespace monopoly::risk {

inline constexpr long kMonopolyUndevelopedMultiplier = 2;  // 2x site for full set

inline long stationRent(int countOwned) {
  switch (countOwned) {
    case 1: return 250000;
    case 2: return 500000;
    case 3: return 1000000;
    case 4: return 2000000;
    default: return 0;
  }
}

inline long utilityRent(int countOwned, int arrivalSum) {
  const long perPip = (countOwned >= 2) ? 100000 : 40000;  // 10x / 4x dice, x10000
  return perPip * static_cast<long>(arrivalSum);
}

}  // namespace monopoly::risk
```

- [ ] **Step 2: `src/risk/rent_table.h`**

```cpp
#pragma once
#include "domain/game_state.h"

namespace monopoly::risk {

// Rent a non-owner owes for ending their move on `pos`, given the board state and
// the dice sum that brought them there (only utilities use arrivalSum).
// Returns 0 if unowned, owned by `mover`, mortgaged, or not a property square.
long rentOwed(const domain::GameState& gs, int pos, int mover, int arrivalSum);

}  // namespace monopoly::risk
```

- [ ] **Step 3: `src/risk/rent_table.cpp`**

```cpp
#include "risk/rent_table.h"

#include "domain/square.h"
#include "risk/rent_model.h"

namespace monopoly::risk {

using domain::ColorGroup;
using domain::SquareType;

long rentOwed(const domain::GameState& gs, int pos, int mover, int arrivalSum) {
  const auto& sq = gs.board().at(pos);
  if (!domain::isPurchasable(sq.type)) return 0;
  const int owner = gs.ownerOf(pos);
  if (owner == domain::kUnowned || owner == mover) return 0;
  if (gs.isMortgaged(pos)) return 0;

  switch (sq.type) {
    case SquareType::Street: {
      const int h = gs.housesOn(pos);
      if (h == 0) {
        const long site = sq.rent[0];
        return gs.ownsWholeGroup(owner, sq.group)
                   ? site * kMonopolyUndevelopedMultiplier
                   : site;
      }
      const std::size_t idx = (h > 5) ? 5 : static_cast<std::size_t>(h);
      return sq.rent[idx];
    }
    case SquareType::Station:
      return stationRent(gs.countOwnedInGroup(owner, ColorGroup::Station));
    case SquareType::Utility:
      return utilityRent(gs.countOwnedInGroup(owner, ColorGroup::Utility), arrivalSum);
    default:
      return 0;
  }
}

}  // namespace monopoly::risk
```

- [ ] **Step 4: `tests/unit/test_rent.cpp`** — site, monopoly doubling, houses, station-by-count, utility-by-dice, mortgage=0, own-property=0.

```cpp
#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/game_state.h"
#include "risk/rent_table.h"
#include "risk/rent_model.h"

using namespace monopoly::domain;
using monopoly::risk::rentOwed;

TEST(Rent, StreetSiteMonopolyAndHouses) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int owner = gs.addPlayer("P1", 0);
  int mover = gs.addPlayer("P2", 0);
  const int pos = 1;  // Portobello Road Market = Old Kent Road analog
  gs.setOwner(pos, owner);
  EXPECT_EQ(rentOwed(gs, pos, mover, 7), 20000);          // site (£2 x10000)
  gs.setOwner(3, owner);                                   // complete BROWN
  EXPECT_EQ(rentOwed(gs, pos, mover, 7), 40000);           // undeveloped monopoly (2x)
  gs.setHouses(pos, 1);
  EXPECT_EQ(rentOwed(gs, pos, mover, 7), 100000);          // 1 house (£10 x10000)
}

TEST(Rent, MortgageAndOwnerExemptions) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int owner = gs.addPlayer("P1", 0);
  int mover = gs.addPlayer("P2", 0);
  gs.setOwner(1, owner);
  gs.setMortgaged(1, true);
  EXPECT_EQ(rentOwed(gs, 1, mover, 7), 0);                // mortgaged
  gs.setMortgaged(1, false);
  EXPECT_EQ(rentOwed(gs, 1, owner, 7), 0);                // owner pays no rent
}

TEST(Rent, StationByCountAndUtilityByDice) {
  auto board = loadBoardFromFile(BOARD_JSON_PATH);
  GameState gs(board);
  int owner = gs.addPlayer("P1", 0);
  int mover = gs.addPlayer("P2", 0);
  gs.setOwner(5, owner);                                  // 1 station
  EXPECT_EQ(rentOwed(gs, 5, mover, 7), 250000);
  gs.setOwner(15, owner);                                 // 2 stations
  EXPECT_EQ(rentOwed(gs, 5, mover, 7), 500000);
  gs.setOwner(12, owner);                                 // 1 utility
  EXPECT_EQ(rentOwed(gs, 12, mover, 9), 40000 * 9);
}
```

- [ ] **Step 5:** Build + `ctest -R Rent` → PASS. **Commit** `feat(risk): price-derived rent model + rent table`.

---

### Task 2: InsurancePricer (single-roll liability + mugging EV)

**Files:** Create `src/pricing/insurance_pricer.h` / `.cpp`, `tests/unit/test_pricing.cpp`; modify `CMakeLists.txt`.

- [ ] **Step 1: `src/pricing/insurance_pricer.h`**

```cpp
#pragma once
#include "domain/game_state.h"
#include "probability/landing.h"

namespace monopoly::pricing {

struct PricingConfig {
  bool muggingEnabled = false;
  long muggingAmount = 500000;  // £ transferred when the mugger wins
};

struct RollQuote {
  double expectedRent = 0.0;      // fair premium to insure next-roll rent liability
  double muggingExposure = 0.0;   // signed EV from mugging (positive = expected gain)
  double fairPremium = 0.0;       // expectedRent net of mugging benefit (>=0, floored)
};

// Prices the next single roll for `mover` from their current position, using the
// known positions/ownership in `gs`. Exact for one roll (positions are known).
RollQuote priceNextRoll(const domain::GameState& gs, int mover,
                        const probability::LandingResolver& resolver,
                        const PricingConfig& cfg);

}  // namespace monopoly::pricing
```

- [ ] **Step 2: `src/pricing/insurance_pricer.cpp`**

```cpp
#include "pricing/insurance_pricer.h"

#include <algorithm>
#include "probability/dice.h"
#include "probability/micro_state.h"
#include "risk/rent_table.h"

namespace monopoly::pricing {

RollQuote priceNextRoll(const domain::GameState& gs, int mover,
                        const probability::LandingResolver& resolver,
                        const PricingConfig& cfg) {
  const int from = gs.player(mover).position;
  const double pMuggerWins = probability::pMuggerWins();
  const double pMuggeeEscapes = probability::pMuggeeEscapes();

  RollQuote q;
  // Enumerate the 36 equally-likely dice outcomes; arrivalSum is known per outcome,
  // so utility rent is exact.
  for (int d1 = 1; d1 <= 6; ++d1) {
    for (int d2 = 1; d2 <= 6; ++d2) {
      const int sum = d1 + d2;
      const int raw = (from + sum) % probability::kNumPositions;
      for (const auto& lp : resolver.resolve(raw)) {
        if (lp.position == probability::kJailSentinel) continue;  // no rent in jail
        const double w = (1.0 / 36.0) * lp.prob;
        q.expectedRent +=
            w * static_cast<double>(risk::rentOwed(gs, lp.position, mover, sum));
        if (cfg.muggingEnabled) {
          const int occ = gs.occupantAt(lp.position, /*exclude=*/mover);
          const auto& sq = gs.board().at(lp.position);
          const bool eligible = sq.type != domain::SquareType::Jail &&
                                sq.type != domain::SquareType::FreeParking;
          if (occ != domain::kUnowned && eligible) {
            // Mover is the mugger: wins -> +amount; loses -> goes to jail (no cash).
            q.muggingExposure +=
                w * (pMuggerWins * static_cast<double>(cfg.muggingAmount) -
                     pMuggeeEscapes * 0.0);
          }
        }
      }
    }
  }
  q.fairPremium = std::max(0.0, q.expectedRent - std::max(0.0, q.muggingExposure));
  return q;
}

}  // namespace monopoly::pricing
```

- [ ] **Step 3: `tests/unit/test_pricing.cpp`**

```cpp
#include <gtest/gtest.h>
#include "domain/board_factory.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "pricing/insurance_pricer.h"
#include "probability/landing.h"

using namespace monopoly;
using domain::GameState;

TEST(Pricing, NoOpponentPropertyMeansZeroPremium) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  GameState gs(board);
  int mover = gs.addPlayer("P1", 0);
  gs.player(mover).position = 0;
  pricing::PricingConfig cfg;
  auto q = pricing::priceNextRoll(gs, mover, resolver, cfg);
  EXPECT_NEAR(q.expectedRent, 0.0, 1e-6);
  EXPECT_NEAR(q.fairPremium, 0.0, 1e-6);
}

TEST(Pricing, PremiumRisesWithDevelopedOpponentProperty) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  GameState gs(board);
  int mover = gs.addPlayer("P1", 0);
  int opp = gs.addPlayer("P2", 0);
  gs.player(mover).position = 18;          // 7 ahead is 25 (Gatwick), but set up reds
  // Opponent owns a hotel on a square reachable in one roll: 24 from 18 is 6 ahead.
  gs.setOwner(24, opp);
  gs.setOwner(21, opp); gs.setOwner(23, opp);   // complete RED monopoly
  auto qNoHouse = pricing::priceNextRoll(gs, mover, resolver, pricing::PricingConfig{});
  gs.setHouses(24, 5);                      // hotel
  auto qHotel = pricing::priceNextRoll(gs, mover, resolver, pricing::PricingConfig{});
  EXPECT_GT(qHotel.expectedRent, qNoHouse.expectedRent);
}

TEST(Pricing, MuggingBenefitReducesPremium) {
  auto board = domain::loadBoardFromFile(BOARD_JSON_PATH);
  auto decks = domain::loadDecksFromFile(DECKS_JSON_PATH);
  probability::LandingResolver resolver(board, decks);
  GameState gs(board);
  int mover = gs.addPlayer("P1", 0);
  int opp = gs.addPlayer("P2", 0);
  gs.player(mover).position = 18;
  gs.player(opp).position = 24;             // opponent standing 6 ahead
  pricing::PricingConfig cfg; cfg.muggingEnabled = true;
  auto q = pricing::priceNextRoll(gs, mover, resolver, cfg);
  EXPECT_GT(q.muggingExposure, 0.0);        // mover expects to gain by mugging
}
```

- [ ] **Step 4:** Build + `ctest -R Pricing` → PASS. **Commit** `feat(pricing): single-roll insurance pricer with mugging EV`.

---

### Task 3: Wire risk/pricing into the demo

**Files:** Modify `src/app/main.cpp`.

- [ ] **Step 1:** Build a small demo `GameState` (2 players, a couple of owned properties), print the next-roll fair premium for the mover alongside the landing distribution. Keep additions under the file cap.

- [ ] **Step 2:** Build, run `./build/monopoly`, eyeball a non-zero premium when the mover can land on opponent property. **Commit** `feat(app): show next-roll insurance quote in demo`.

---

## Milestone 4 acceptance
- `ctest` green; rent table correct across street/station/utility/mortgage/owner cases;
  pricer is exact per single roll, rises with development, and nets mugging benefit.
- Demo prints a fair next-roll premium. All files < 400 LoC.
