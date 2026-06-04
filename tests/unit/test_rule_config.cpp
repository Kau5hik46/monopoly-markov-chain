#include <gtest/gtest.h>
#include "rules/rule_config.h"

#ifndef RULES_JSON_PATH
#define RULES_JSON_PATH "data/rules.default.json"
#endif

using namespace monopoly::rules;

TEST(RuleConfig, BuilderSetsFields) {
  RuleConfig c = RuleConfigBuilder()
                     .mugging(true, 750000)
                     .airportTravel(false)
                     .freeParkingPot(true, 3, 2)
                     .passGoBonus(2500000)
                     .build();
  EXPECT_TRUE(c.muggingEnabled);
  EXPECT_EQ(c.muggingAmount, 750000);
  EXPECT_FALSE(c.airportTravelEnabled);
  EXPECT_EQ(c.housesPerIncomeTax, 3);
  EXPECT_EQ(c.housesPerSuperTax, 2);
  EXPECT_EQ(c.passGoBonus, 2500000);
}

TEST(RuleConfig, LoaderReadsDefaults) {
  RuleConfig c = loadRuleConfig(RULES_JSON_PATH);
  EXPECT_TRUE(c.muggingEnabled);
  EXPECT_EQ(c.muggingAmount, 500000);
  EXPECT_EQ(c.hospitalPosition, 20);
  EXPECT_TRUE(c.freeParkingPotEnabled);
  EXPECT_EQ(c.housesPerIncomeTax, 2);
  EXPECT_EQ(c.housesPerSuperTax, 1);
}
