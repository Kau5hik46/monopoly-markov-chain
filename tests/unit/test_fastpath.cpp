#include <gtest/gtest.h>
#include "engine/fastpath.h"

using namespace monopoly::engine;

namespace {
std::vector<MenuAction> menuOf(std::vector<std::string> prompts) {
  return buildMenu(prompts);
}
}  // namespace

TEST(FastPath, DiceShorthandRollsCurrentPlayer) {
  auto fi = interpret("3,4", {}, /*currentRoller=*/0);
  EXPECT_EQ(fi.kind, FastInput::Kind::Run);
  EXPECT_EQ(fi.text, "roll P1 = 3,4");
  auto sp = interpret("5 2", {}, /*currentRoller=*/2);
  EXPECT_EQ(sp.kind, FastInput::Kind::Run);
  EXPECT_EQ(sp.text, "roll P3 = 5,2");
}

TEST(FastPath, DiceRejectsOutOfRangeOrNoSeparator) {
  EXPECT_EQ(interpret("7,1", {}, 0).kind, FastInput::Kind::None);  // 7 invalid
  EXPECT_EQ(interpret("34", {}, 0).kind, FastInput::Kind::None);   // no separator -> not dice
}

TEST(FastPath, BuildMenuExcludesRollsAndClassifies) {
  auto m = menuOf({
      "buy P2 @#1   (MEDITERRANEAN AVENUE, $60)  — or skip",
      "roll P1 = d1,d2   (rolls again)",
      "tax P1 = <amount> @INCOME",
  });
  ASSERT_EQ(m.size(), 2u);                       // roll excluded
  EXPECT_EQ(m[0].command, "buy P2 @#1");
  EXPECT_TRUE(m[0].skippable);
  EXPECT_TRUE(m[0].moneyMoving);
  EXPECT_TRUE(m[1].hasPlaceholder);              // tax
  EXPECT_FALSE(m[1].skippable);                  // mandatory
}

TEST(FastPath, DefaultIsFirstMandatoryElseSkip) {
  auto onlyOptional = menuOf({"buy P2 @#1   — or skip"});
  EXPECT_EQ(defaultActionIndex(onlyOptional), -1);          // Enter skips
  auto hasMandatory = menuOf({"buy P2 @#1 — or skip", "settle all"});
  EXPECT_EQ(defaultActionIndex(hasMandatory), 1);           // settle is default
}

TEST(FastPath, EnterSkipsWhenAllOptional) {
  auto m = menuOf({"buy P2 @#1   — or skip"});
  EXPECT_EQ(interpret("", m, 0).kind, FastInput::Kind::None);  // blank = skip
}

TEST(FastPath, EnterRunsMandatorySafeAction) {
  auto m = menuOf({"settle all"});
  auto fi = interpret("", m, 0);
  EXPECT_EQ(fi.kind, FastInput::Kind::Run);
  EXPECT_EQ(fi.text, "settle all");
}

TEST(FastPath, DigitAcceptsMoneyActionAsPrefill) {
  auto m = menuOf({"buy P2 @#1   (MEDITERRANEAN AVENUE, $60)  — or skip"});
  auto fi = interpret("1", m, 0);
  EXPECT_EQ(fi.kind, FastInput::Kind::Prefill);   // money-moving -> confirm
  EXPECT_EQ(fi.text, "buy P2 @#1");
}

TEST(FastPath, PlaceholderActionPrefillsUpToPlaceholder) {
  auto m = menuOf({"mug P1 vs P2 = <muggerTotal>:<muggeeTotal>"});
  auto fi = interpret("1", m, 0);
  EXPECT_EQ(fi.kind, FastInput::Kind::Prefill);
  EXPECT_EQ(fi.text, "mug P1 vs P2 = ");          // type only the numbers
}

TEST(FastPath, NormalCommandIsPassedThrough) {
  auto m = menuOf({"settle all"});
  EXPECT_EQ(interpret("buy P1 @#3", m, 0).kind, FastInput::Kind::None);
  EXPECT_EQ(interpret("query state", m, 0).kind, FastInput::Kind::None);
}
