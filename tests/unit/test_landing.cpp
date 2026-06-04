#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "probability/landing.h"
#include "probability/micro_state.h"
#include "domain/board_factory.h"
#include "domain/deck_factory.h"

using namespace monopoly::probability;
using namespace monopoly::domain;

namespace {
double total(const std::vector<LandingProb>& v) {
  double s = 0; for (auto& l : v) s += l.prob; return s;
}
double massAt(const std::vector<LandingProb>& v, int pos) {
  for (auto& l : v) if (l.position == pos) return l.prob; return 0.0;
}
}  // namespace

TEST(Landing, PlainSquareIsTerminal) {
  auto b = loadBoardFromFile(BOARD_JSON_PATH);
  auto d = loadDecksFromFile(DECKS_JSON_PATH);
  LandingResolver r(b, d);
  auto out = r.resolve(1);  // Portobello Road Market
  EXPECT_NEAR(total(out), 1.0, 1e-12);
  EXPECT_NEAR(massAt(out, 1), 1.0, 1e-12);
}

TEST(Landing, GoToJailRedirects) {
  auto b = loadBoardFromFile(BOARD_JSON_PATH);
  auto d = loadDecksFromFile(DECKS_JSON_PATH);
  LandingResolver r(b, d);
  auto out = r.resolve(30);
  EXPECT_NEAR(massAt(out, kJailSentinel), 1.0, 1e-12);
}

TEST(Landing, ChanceDistributesAndConserves) {
  auto b = loadBoardFromFile(BOARD_JSON_PATH);
  auto d = loadDecksFromFile(DECKS_JSON_PATH);
  LandingResolver r(b, d);
  auto out = r.resolve(7);  // Chance
  EXPECT_NEAR(total(out), 1.0, 1e-12);
  // 6/16 "stay" mass remains on square 7.
  EXPECT_NEAR(massAt(out, 7), 6.0 / 16.0, 1e-12);
  // 1/16 advance to GO(0); 2/16 nearest station from 7 == 15.
  EXPECT_NEAR(massAt(out, 0), 1.0 / 16.0, 1e-12);
  EXPECT_GE(massAt(out, 15), 2.0 / 16.0 - 1e-12);
  // some jail mass from the go-to-jail card.
  EXPECT_GE(massAt(out, kJailSentinel), 1.0 / 16.0 - 1e-12);
}
