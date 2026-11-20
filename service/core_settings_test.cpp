// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "core_settings.hpp"

#include <gtest/gtest.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "agent_facts.hpp"

namespace cs = nsclient::core::core_settings;
using fact_repository = nsclient::core::fact_repository;
using failed_check_store = nsclient::core::failed_check_store;

namespace {

// A section as an operator wrote it: only the keys that were set.
cs::reader section(const std::map<std::string, std::string> &values) {
  return [values](const std::string &key, const std::string &fallback) {
    const auto found = values.find(key);
    return found == values.end() ? fallback : found->second;
  };
}

failed_check_store::options read(const std::map<std::string, std::string> &values, std::vector<std::string> &errors,
                                 const failed_check_store::options &in_force = failed_check_store::options()) {
  return cs::read_failed_checks(section(values), in_force, errors);
}

bool mentions(const std::vector<std::string> &errors, const std::string &text) {
  for (const std::string &error : errors) {
    if (error.find(text) != std::string::npos) return true;
  }
  return false;
}

// What the scheduler does with an interval, as far as the helper can tell.
struct fake_scheduler {
  std::vector<std::string> added;
  void operator()(const std::string &interval) {
    if (interval.find("bogus") != std::string::npos) throw std::invalid_argument("not a duration: " + interval);
    added.push_back(interval);
  }
};

// A fact set of a known size: the `agent` set with one long module name.
std::string store_set(fact_repository &facts, const std::string &name, const std::size_t padding) {
  std::string error;
  facts.set(name, fact_repository::core_owner, nsclient::core::agent_facts::build("1.0", {std::string(padding, 'x')}, false), error);
  return error;
}

}  // namespace

// --- schedule_or_fallback ----------------------------------------------------

TEST(CoreSettingsSchedule, AUsableIntervalIsScheduledAsGiven) {
  fake_scheduler scheduler;
  std::vector<std::string> errors;
  EXPECT_TRUE(cs::schedule_or_fallback(std::ref(scheduler), "'metrics interval'", "30s", "10s", errors));
  EXPECT_EQ(scheduler.added, std::vector<std::string>({"30s"}));
  EXPECT_TRUE(errors.empty());
}

TEST(CoreSettingsSchedule, AnUnusableIntervalSchedulesTheDefaultAndSaysSo) {
  // The alternative - no task at all - is an agent that never collects
  // metrics again because of a typo.
  fake_scheduler scheduler;
  std::vector<std::string> errors;
  EXPECT_FALSE(cs::schedule_or_fallback(std::ref(scheduler), "'metrics interval'", "bogus", "10s", errors));
  EXPECT_EQ(scheduler.added, std::vector<std::string>({"10s"}));
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_EQ(errors.front(), "Invalid 'metrics interval' value 'bogus', falling back to '10s': not a duration: bogus");
}

// --- apply_facts_max_size ----------------------------------------------------

TEST(CoreSettingsFactsMaxSize, AUsableCapIsApplied) {
  fact_repository facts;
  std::vector<std::string> errors;
  cs::apply_facts_max_size(facts, "4096", errors);
  EXPECT_EQ(facts.get_max_size(), 4096u);
  EXPECT_TRUE(errors.empty());
}

TEST(CoreSettingsFactsMaxSize, ACapThatIsNotANumberKeepsThePreviousOne) {
  fact_repository facts;
  std::vector<std::string> errors;
  cs::apply_facts_max_size(facts, "4096", errors);
  for (const std::string text : {"lots", "", "1mb"}) {
    errors.clear();
    cs::apply_facts_max_size(facts, text, errors);
    EXPECT_EQ(facts.get_max_size(), 4096u) << text;
    ASSERT_EQ(errors.size(), 1u) << text;
    EXPECT_NE(errors.front().find("Invalid facts 'max size' value '" + text + "', keeping the previous one"), std::string::npos) << errors.front();
  }
}

TEST(CoreSettingsFactsMaxSize, ASignedOrPaddedCapKeepsThePreviousOne) {
  // lexical_cast reads "-1" as the largest size_t, which removed the cap.
  fact_repository facts;
  std::vector<std::string> errors;
  cs::apply_facts_max_size(facts, "4096", errors);
  for (const std::string text : {"-1", "+5", " 5", "5 ", "99999999999999999999999"}) {
    errors.clear();
    cs::apply_facts_max_size(facts, text, errors);
    EXPECT_EQ(facts.get_max_size(), 4096u) << text;
    ASSERT_EQ(errors.size(), 1u) << text;
    EXPECT_NE(errors.front().find("Invalid facts 'max size' value '" + text + "', keeping the previous one: not a non-negative whole number"),
              std::string::npos)
        << errors.front();
  }
}

TEST(CoreSettingsFactsMaxSize, ACapTooSmallForAnySetKeepsThePreviousOne) {
  // 0 or 1 would not hold even an empty set, so every set would be dropped and
  // rejected forever: a way of switching facts off that reads like a typo.
  fact_repository facts;
  std::vector<std::string> errors;
  cs::apply_facts_max_size(facts, "0", errors);
  EXPECT_EQ(facts.get_max_size(), fact_repository::default_max_size);
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_NE(errors.front().find("too small to hold any fact set"), std::string::npos) << errors.front();
  EXPECT_NE(errors.front().find(std::to_string(fact_repository::default_max_size)), std::string::npos) << errors.front();
}

TEST(CoreSettingsFactsMaxSize, TheSmallestUsableCapIsAccepted) {
  fact_repository facts;
  std::vector<std::string> errors;
  cs::apply_facts_max_size(facts, std::to_string(fact_repository::min_max_size), errors);
  EXPECT_EQ(facts.get_max_size(), fact_repository::min_max_size);
  EXPECT_TRUE(errors.empty());
}

TEST(CoreSettingsFactsMaxSize, LoweringTheCapUnderTheDocumentNamesEverySetItDrops) {
  fact_repository facts;
  ASSERT_EQ(store_set(facts, "small", 10), "");
  ASSERT_EQ(store_set(facts, "large", 2000), "");
  std::vector<std::string> errors;

  // Room for the small one only.
  cs::apply_facts_max_size(facts, "1000", errors);
  EXPECT_EQ(facts.get_max_size(), 1000u);
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_NE(errors.front().find("Dropped the fact set 'large'"), std::string::npos) << errors.front();
  EXPECT_NE(errors.front().find("max size of 1000 bytes"), std::string::npos) << errors.front();

  // Raising it again drops nothing and has nothing to say.
  errors.clear();
  cs::apply_facts_max_size(facts, "100000", errors);
  EXPECT_TRUE(errors.empty());
}

// --- read_failed_checks ------------------------------------------------------

TEST(CoreSettingsFailedChecks, AnEmptySectionIsTheDefaults) {
  std::vector<std::string> errors;
  const failed_check_store::options options = read({}, errors);
  const failed_check_store::options defaults;
  EXPECT_TRUE(errors.empty());
  EXPECT_EQ(options.enabled, defaults.enabled);
  EXPECT_EQ(options.max_entries, defaults.max_entries);
  EXPECT_EQ(options.record_arguments, defaults.record_arguments);
  EXPECT_EQ(options.statuses, defaults.statuses);
  EXPECT_EQ(options.capture_metrics, defaults.capture_metrics);
  EXPECT_EQ(options.context_budget, defaults.context_budget);
  EXPECT_EQ(options.max_context_size, defaults.max_context_size);
  EXPECT_FALSE(options.prune.any());
  EXPECT_EQ(options.capture_diagnostics, defaults.capture_diagnostics);
  EXPECT_EQ(options.diagnostics_rows, defaults.diagnostics_rows);
}

TEST(CoreSettingsFailedChecks, AnEmptySectionResetsWhatAnEarlierOneSet) {
  // A reload after the operator removed a key has to bring the default back,
  // not leave the removed value in force: the configuration in force is only
  // what an unreadable value falls back to.
  std::vector<std::string> errors;
  const failed_check_store::options before = read({{"enabled", "false"},
                                                   {"max entries", "7"},
                                                   {"record arguments", "all"},
                                                   {"statuses", "critical"},
                                                   {"capture metrics", "true"},
                                                   {"prune metrics", "zeros"},
                                                   {"prune pattern", "Hyper-V"},
                                                   {"capture diagnostics", "true"},
                                                   {"diagnostics rows", "3"}},
                                                  errors);
  ASSERT_TRUE(errors.empty());
  const failed_check_store::options after = read({}, errors, before);
  const failed_check_store::options defaults;
  EXPECT_TRUE(errors.empty());
  EXPECT_TRUE(after.enabled);
  EXPECT_EQ(after.max_entries, defaults.max_entries);
  EXPECT_EQ(after.record_arguments, failed_check_store::argument_mode::safe);
  EXPECT_EQ(after.statuses, defaults.statuses);
  EXPECT_FALSE(after.capture_metrics);
  EXPECT_FALSE(after.prune.any());
  EXPECT_FALSE(after.capture_diagnostics);
  EXPECT_EQ(after.diagnostics_rows, defaults.diagnostics_rows);
}

TEST(CoreSettingsFailedChecks, EveryKeyIsRead) {
  std::vector<std::string> errors;
  const failed_check_store::options options = read({{"enabled", "false"},
                                                    {"max entries", "7"},
                                                    {"record arguments", "all"},
                                                    {"statuses", "critical, unknown"},
                                                    {"capture metrics", "true"},
                                                    {"metrics context budget", "100000"},
                                                    {"max context size", "5000"},
                                                    {"prune metrics", "zeros,duplicates"},
                                                    {"prune pattern", "Hyper-V Virtual, HarddiskVolume8"},
                                                    {"capture diagnostics", "true"},
                                                    {"diagnostics rows", "3"}},
                                                   errors);
  EXPECT_TRUE(errors.empty());
  EXPECT_FALSE(options.enabled);
  EXPECT_EQ(options.max_entries, 7u);
  EXPECT_EQ(options.record_arguments, failed_check_store::argument_mode::all);
  EXPECT_EQ(failed_check_store::format_statuses(options.statuses), "critical,unknown");
  EXPECT_TRUE(options.capture_metrics);
  EXPECT_EQ(options.context_budget, 100000u);
  EXPECT_EQ(options.max_context_size, 5000u);
  EXPECT_TRUE(options.prune.zeros);
  EXPECT_FALSE(options.prune.empty);
  EXPECT_TRUE(options.prune.duplicates);
  // Lower-cased, which is what the match is made against.
  EXPECT_EQ(options.prune.patterns, std::vector<std::string>({"hyper-v virtual", "harddiskvolume8"}));
  EXPECT_TRUE(options.capture_diagnostics);
  EXPECT_EQ(options.diagnostics_rows, 3u);
}

TEST(CoreSettingsFailedChecks, TheBooleanSpellingsOfRecordArgumentsKeepTheirMeaning) {
  // The key was a boolean before it took a mode.
  std::vector<std::string> errors;
  EXPECT_EQ(read({{"record arguments", "true"}}, errors).record_arguments, failed_check_store::argument_mode::all);
  EXPECT_EQ(read({{"record arguments", "false"}}, errors).record_arguments, failed_check_store::argument_mode::none);
  EXPECT_EQ(read({{"record arguments", "none"}}, errors).record_arguments, failed_check_store::argument_mode::none);
  EXPECT_TRUE(errors.empty());
}

TEST(CoreSettingsFailedChecks, ACountThatIsNotANumberKeepsWhatWasInForce) {
  failed_check_store::options in_force;
  in_force.max_entries = 123;
  in_force.context_budget = 200000;
  in_force.max_context_size = 3000;
  in_force.diagnostics_rows = 4;
  std::vector<std::string> errors;
  const failed_check_store::options options =
      read({{"max entries", "lots"}, {"metrics context budget", "4mb"}, {"max context size", ""}, {"diagnostics rows", "few"}}, errors, in_force);
  EXPECT_EQ(options.max_entries, 123u);
  EXPECT_EQ(options.context_budget, 200000u);
  EXPECT_EQ(options.max_context_size, 3000u);
  EXPECT_EQ(options.diagnostics_rows, 4u);
  ASSERT_EQ(errors.size(), 4u);
  EXPECT_TRUE(mentions(errors, "Invalid [/settings/core/failed checks] max entries 'lots', keeping 123"));
  EXPECT_TRUE(mentions(errors, "Invalid [/settings/core/failed checks] metrics context budget '4mb', keeping 200000"));
  EXPECT_TRUE(mentions(errors, "Invalid [/settings/core/failed checks] max context size '', keeping 3000"));
  EXPECT_TRUE(mentions(errors, "Invalid [/settings/core/failed checks] diagnostics rows 'few', keeping 4"));
}

TEST(CoreSettingsFailedChecks, ASignedOrPaddedCountKeepsWhatWasInForce) {
  // lexical_cast reads "-1" as the largest size_t: no limit at all.
  failed_check_store::options in_force;
  in_force.max_entries = 123;
  in_force.diagnostics_rows = 4;
  for (const std::string text : {"-1", "+5", " 5"}) {
    std::vector<std::string> errors;
    const failed_check_store::options options = read({{"max entries", text}, {"diagnostics rows", text}}, errors, in_force);
    EXPECT_EQ(options.max_entries, 123u) << text;
    EXPECT_EQ(options.diagnostics_rows, 4u) << text;
    ASSERT_EQ(errors.size(), 2u) << text;
    EXPECT_TRUE(mentions(errors, "Invalid [/settings/core/failed checks] max entries '" + text + "', keeping 123: not a non-negative whole number")) << text;
    EXPECT_TRUE(mentions(errors, "Invalid [/settings/core/failed checks] diagnostics rows '" + text + "', keeping 4")) << text;
  }
}

TEST(CoreSettingsFailedChecks, AnUnknownArgumentModeKeepsWhatWasInForce) {
  failed_check_store::options in_force;
  in_force.record_arguments = failed_check_store::argument_mode::none;
  std::vector<std::string> errors;
  const failed_check_store::options options = read({{"record arguments", "some"}}, errors, in_force);
  // Not `safe`, and above all not `all`: an unreadable value must never start
  // recording arguments an operator had turned off.
  EXPECT_EQ(options.record_arguments, failed_check_store::argument_mode::none);
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_NE(errors.front().find("record arguments 'some', keeping 'none'"), std::string::npos) << errors.front();
}

TEST(CoreSettingsFailedChecks, AnUnknownStatusKeepsWhatWasInForce) {
  std::vector<std::string> errors;
  failed_check_store::options in_force = read({{"statuses", "critical"}}, errors);
  ASSERT_TRUE(errors.empty());
  // `ok` is the interesting one: it is a status, just never a failed check.
  for (const std::string text : {"critical,ok", "bad", ","}) {
    errors.clear();
    const failed_check_store::options options = read({{"statuses", text}}, errors, in_force);
    EXPECT_EQ(failed_check_store::format_statuses(options.statuses), "critical") << text;
    ASSERT_EQ(errors.size(), 1u) << text;
    EXPECT_NE(errors.front().find("statuses '" + text + "', keeping 'critical'"), std::string::npos) << errors.front();
  }
}

TEST(CoreSettingsFailedChecks, AnUnknownPruneRuleKeepsTheRulesButStillReadsThePatterns) {
  std::vector<std::string> errors;
  const failed_check_store::options in_force = read({{"prune metrics", "empty"}, {"prune pattern", "old"}}, errors);
  ASSERT_TRUE(errors.empty());
  const failed_check_store::options options = read({{"prune metrics", "zeros,everything"}, {"prune pattern", "new"}}, errors, in_force);
  EXPECT_FALSE(options.prune.zeros) << "half of a list that did not parse was applied";
  EXPECT_TRUE(options.prune.empty);
  EXPECT_EQ(options.prune.patterns, std::vector<std::string>({"new"}));
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_NE(errors.front().find("prune metrics 'zeros,everything', keeping 'empty'"), std::string::npos) << errors.front();
}

TEST(CoreSettingsFailedChecks, DiagnosticsRowsAreHeldToWhatATableCanCarry) {
  std::vector<std::string> errors;
  EXPECT_EQ(read({{"diagnostics rows", "0"}}, errors).diagnostics_rows, 1u);
  EXPECT_EQ(read({{"diagnostics rows", "100000"}}, errors).diagnostics_rows, nscapi::diagnostics::max_rows);
  EXPECT_TRUE(errors.empty());
}

TEST(CoreSettingsFailedChecks, OneBadKeyDoesNotStopTheRestFromApplying) {
  std::vector<std::string> errors;
  const failed_check_store::options options =
      read({{"max entries", "lots"}, {"enabled", "false"}, {"statuses", "warning"}, {"capture metrics", "true"}}, errors);
  EXPECT_EQ(errors.size(), 1u);
  EXPECT_FALSE(options.enabled);
  EXPECT_TRUE(options.capture_metrics);
  EXPECT_EQ(failed_check_store::format_statuses(options.statuses), "warning");
}

TEST(CoreSettingsFailedChecks, WhatWasReadIsWhatTheStoreAccepts) {
  // The join with the store: the limits the reader leaves alone are applied by
  // configure(), so an absurd value ends up clamped rather than in force.
  std::vector<std::string> errors;
  failed_check_store store;
  store.configure(read({{"max entries", "0"}, {"metrics context budget", "1"}, {"max context size", "999999999"}}, errors));
  EXPECT_TRUE(errors.empty());
  const failed_check_store::options applied = store.get_options();
  EXPECT_EQ(applied.max_entries, 1u);
  EXPECT_EQ(applied.context_budget, failed_check_store::min_context_budget);
  EXPECT_EQ(applied.max_context_size, failed_check_store::min_context_budget);
}
