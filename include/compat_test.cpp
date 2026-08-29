// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for the legacy (CheckSystem 0.4.x style) compatibility helpers.
 *
 * Coverage:
 *   - addShowAll / addAllNumeric / addOldNumeric option registration
 *   - do_matchFirstNumeric / matchFirstNumeric / matchFirstOldNumeric
 *     translation of MaxWarn/MinCrit/... bounds into modern filter expressions
 *   - hasFirstNumeric
 *   - matchShowAll argument forwarding
 *   - inline_addarg
 *   - log_args (smoke: builds the quoted command line)
 */

#include <gtest/gtest.h>

#include <boost/program_options.hpp>
#include <compat.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>
#include <vector>

// Unit-test binaries have no generated module glue, so define the plugin
// singleton normally provided by NSC_WRAP_DLL().
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace po = boost::program_options;

namespace {

/// Parse `args` against `desc` into a variables_map.
po::variables_map parse(const po::options_description &desc, const std::vector<std::string> &args) {
  po::variables_map vm;
  po::store(po::command_line_parser(args).options(desc).run(), vm);
  po::notify(vm);
  return vm;
}

po::options_description numeric_desc(const std::string &suffix = "") {
  po::options_description desc("test");
  compat::addAllNumeric(desc, suffix);
  return desc;
}

}  // namespace

// =============================================================================
// option registration
// =============================================================================

TEST(Compat, AddShowAllRegistersOptionWithImplicitValue) {
  po::options_description desc("test");
  compat::addShowAll(desc);

  po::variables_map vm = parse(desc, {"--ShowAll"});
  ASSERT_EQ(vm.count("ShowAll"), 1u);
  EXPECT_EQ(vm["ShowAll"].as<std::string>(), "short");

  po::variables_map vm2 = parse(desc, {"--ShowAll", "long"});
  EXPECT_EQ(vm2["ShowAll"].as<std::string>(), "long");
}

TEST(Compat, AddAllNumericRegistersAllFourBounds) {
  po::options_description desc = numeric_desc();
  po::variables_map vm = parse(desc, {"--MaxWarn", "1", "--MaxCrit", "2", "--MinWarn", "3", "--MinCrit", "4"});
  EXPECT_EQ(vm.count("MaxWarn"), 1u);
  EXPECT_EQ(vm.count("MaxCrit"), 1u);
  EXPECT_EQ(vm.count("MinWarn"), 1u);
  EXPECT_EQ(vm.count("MinCrit"), 1u);
}

TEST(Compat, AddAllNumericAppliesSuffix) {
  po::options_description desc = numeric_desc("Free");
  po::variables_map vm = parse(desc, {"--MaxWarnFree", "10"});
  EXPECT_EQ(vm.count("MaxWarnFree"), 1u);
  EXPECT_EQ(vm.count("MaxWarn"), 0u);
}

TEST(Compat, AddOldNumericRegistersWarnAndCrit) {
  po::options_description desc("test");
  compat::addOldNumeric(desc);
  po::variables_map vm = parse(desc, {"--warn", "10", "--crit", "20"});
  EXPECT_EQ(vm.count("warn"), 1u);
  EXPECT_EQ(vm.count("crit"), 1u);
}

// =============================================================================
// do_matchFirstNumeric
// =============================================================================

TEST(Compat, DoMatchBuildsSimpleExpression) {
  po::options_description desc = numeric_desc();
  po::variables_map vm = parse(desc, {"--MaxWarn", "90"});

  std::string target;
  compat::do_matchFirstNumeric(vm, "MaxWarn", target, "warn", "used", ">=");
  EXPECT_EQ(target, "warn=used>=90");
}

TEST(Compat, DoMatchMissingKeyLeavesTargetUntouched) {
  po::options_description desc = numeric_desc();
  po::variables_map vm = parse(desc, {"--MaxCrit", "90"});

  std::string target;
  compat::do_matchFirstNumeric(vm, "MaxWarn", target, "warn", "used", ">=");
  EXPECT_EQ(target, "");
}

TEST(Compat, DoMatchParsesTwoCharOperatorPrefix) {
  po::options_description desc = numeric_desc();
  po::variables_map vm = parse(desc, {"--MaxWarn", "gt:90"});

  std::string target;
  compat::do_matchFirstNumeric(vm, "MaxWarn", target, "warn", "used", ">=");
  EXPECT_EQ(target, "warn=used gt 90");
}

TEST(Compat, DoMatchCombinesSecondBoundWithOr) {
  po::options_description desc = numeric_desc();
  po::variables_map vm = parse(desc, {"--MaxWarn", "90", "--MinWarn", "10"});

  std::string target;
  compat::do_matchFirstNumeric(vm, "MaxWarn", target, "warn", "used", ">=");
  compat::do_matchFirstNumeric(vm, "MinWarn", target, "warn", "free", "<=");
  EXPECT_EQ(target, "warn=( used>=90 ) or ( free<=10 )");
}

TEST(Compat, DoMatchMultipleBoundsUsesFirst) {
  po::options_description desc = numeric_desc();
  po::variables_map vm = parse(desc, {"--MaxWarn", "90", "--MaxWarn", "95"});

  std::string target;
  compat::do_matchFirstNumeric(vm, "MaxWarn", target, "warn", "used", ">=");
  EXPECT_EQ(target, "warn=used>=90");
}

// =============================================================================
// matchFirstNumeric / matchFirstOldNumeric
// =============================================================================

TEST(Compat, MatchFirstNumericMapsMaxAndMin) {
  po::options_description desc = numeric_desc();
  po::variables_map vm = parse(desc, {"--MaxWarn", "80", "--MaxCrit", "90", "--MinWarn", "20", "--MinCrit", "10"});

  std::string warn, crit;
  compat::matchFirstNumeric(vm, "used", "free", warn, crit);
  EXPECT_EQ(warn, "warn=( used>=80 ) or ( free<=20 )");
  EXPECT_EQ(crit, "crit=( used>=90 ) or ( free<=10 )");
}

TEST(Compat, MatchFirstNumericHonoursSuffix) {
  po::options_description desc = numeric_desc("Free");
  po::variables_map vm = parse(desc, {"--MaxWarnFree", "80"});

  std::string warn, crit;
  compat::matchFirstNumeric(vm, "used", "free", warn, crit, "Free");
  EXPECT_EQ(warn, "warn=used>=80");
  EXPECT_EQ(crit, "");
}

TEST(Compat, MatchFirstOldNumericMapsWarnAndCrit) {
  po::options_description desc("test");
  compat::addOldNumeric(desc);
  po::variables_map vm = parse(desc, {"--warn", "5", "--crit", "10"});

  std::string warn, crit;
  compat::matchFirstOldNumeric(vm, "count", warn, crit);
  EXPECT_EQ(warn, "warn=count>=5");
  EXPECT_EQ(crit, "crit=count>=10");
}

// =============================================================================
// hasFirstNumeric
// =============================================================================

TEST(Compat, HasFirstNumericDetectsAnyBound) {
  po::options_description desc = numeric_desc();
  EXPECT_TRUE(compat::hasFirstNumeric(parse(desc, {"--MaxWarn", "1"}), ""));
  EXPECT_TRUE(compat::hasFirstNumeric(parse(desc, {"--MinCrit", "1"}), ""));
  EXPECT_FALSE(compat::hasFirstNumeric(parse(desc, {}), ""));
}

TEST(Compat, HasFirstNumericHonoursSuffix) {
  po::options_description desc = numeric_desc("Free");
  po::variables_map vm = parse(desc, {"--MaxCritFree", "1"});
  EXPECT_TRUE(compat::hasFirstNumeric(vm, "Free"));
  EXPECT_FALSE(compat::hasFirstNumeric(vm, ""));
}

// =============================================================================
// matchShowAll
// =============================================================================

TEST(Compat, MatchShowAllAddsArgumentWhenSet) {
  po::options_description desc("test");
  compat::addShowAll(desc);
  po::variables_map vm = parse(desc, {"--ShowAll"});

  PB::Commands::QueryRequestMessage::Request request;
  compat::matchShowAll(vm, request);
  ASSERT_EQ(request.arguments_size(), 1);
  EXPECT_EQ(request.arguments(0), "show-all");
}

TEST(Compat, MatchShowAllDoesNothingWhenUnset) {
  po::options_description desc("test");
  compat::addShowAll(desc);
  po::variables_map vm = parse(desc, {});

  PB::Commands::QueryRequestMessage::Request request;
  compat::matchShowAll(vm, request);
  EXPECT_EQ(request.arguments_size(), 0);
}

// =============================================================================
// inline_addarg
// =============================================================================

TEST(Compat, InlineAddargSkipsEmptyValues) {
  PB::Commands::QueryRequestMessage::Request request;
  compat::inline_addarg(request, "");
  compat::inline_addarg(request, "prefix=", "");
  EXPECT_EQ(request.arguments_size(), 0);

  compat::inline_addarg(request, "plain");
  compat::inline_addarg(request, "filter=", "x > 1");
  ASSERT_EQ(request.arguments_size(), 2);
  EXPECT_EQ(request.arguments(0), "plain");
  EXPECT_EQ(request.arguments(1), "filter=x > 1");
}

// =============================================================================
// log_args
// =============================================================================

TEST(Compat, LogArgsQuotesArgumentsWithSpaces) {
  // With no core loaded logging is a no-op, but the quoting/joining code still
  // runs; this is a smoke test that it handles spaces and empty requests.
  PB::Commands::QueryRequestMessage::Request request;
  compat::log_args(request);

  request.add_arguments("simple");
  request.add_arguments("has space");
  compat::log_args(request);
}
