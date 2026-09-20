// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "fact_repository.hpp"

#include <gtest/gtest.h>

#include <boost/json.hpp>

using fact_repository = nsclient::core::fact_repository;
using set_result = nsclient::core::fact_repository::set_result;

namespace {
boost::json::value parse(const std::string &json) { return boost::json::parse(json); }

// Store a set and fail the test with the repository's own message if it was
// rejected - a rejection in a test that is not about rejection is a bug in
// the test data, and the message says which.
set_result store(fact_repository &repo, const std::string &name, const unsigned int plugin_id, const std::string &json) {
  std::string error;
  const set_result result = repo.set(name, plugin_id, parse(json), error);
  EXPECT_NE(result, set_result::rejected) << error;
  return result;
}
}  // namespace

TEST(FactRepository, StartsEmptyAtRevisionZero) {
  const fact_repository repo;
  EXPECT_TRUE(repo.get_all().empty());
  EXPECT_EQ(repo.get_revision(), 0u);
  EXPECT_EQ(repo.get_canonical(), "{}");
  EXPECT_TRUE(repo.get_errors().empty());
  EXPECT_TRUE(repo.get_collected().empty());
}

TEST(FactRepository, SetStoresAndBumpsRevision) {
  fact_repository repo;
  EXPECT_EQ(store(repo, "os", 1, R"({"family":"linux","version":"6.1.0"})"), set_result::changed);
  EXPECT_EQ(repo.get_revision(), 1u);
  const boost::json::object all = repo.get_all();
  ASSERT_EQ(all.size(), 1u);
  EXPECT_EQ(all.at("os").as_object().at("family").as_string(), "linux");
}

TEST(FactRepository, ReturningTheSameSetIsANoOp) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(store(repo, "os", 1, R"({"family":"linux"})"), set_result::unchanged);
  EXPECT_EQ(repo.get_revision(), revision) << "an hourly round that finds nothing new must not look like a change";
}

TEST(FactRepository, TheSameFactsInADifferentOrderAreStillTheSameFacts) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux","version":"6.1.0"})");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(store(repo, "os", 1, R"({"version":"6.1.0","family":"linux"})"), set_result::unchanged)
      << "the canonical form is what the server sees, so key order alone is not a change";
  EXPECT_EQ(repo.get_revision(), revision);
}

TEST(FactRepository, ChangingASetBumpsRevision) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux","version":"6.1.0"})");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(store(repo, "os", 1, R"({"family":"linux","version":"6.2.0"})"), set_result::changed);
  EXPECT_EQ(repo.get_revision(), revision + 1);
}

TEST(FactRepository, RemoveDropsTheSetAndRemovingAgainIsANoOp) {
  fact_repository repo;
  store(repo, "docker", 7, R"({"version":"26.1.0"})");
  EXPECT_EQ(repo.remove("docker"), set_result::changed);
  EXPECT_TRUE(repo.get_all().empty());
  EXPECT_EQ(repo.remove("docker"), set_result::unchanged);
}

TEST(FactRepository, UnloadingAModuleTakesItsSetsWithIt) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  store(repo, "docker", 7, R"({"version":"26.1.0"})");
  repo.remove_owned_by(7);
  const boost::json::object all = repo.get_all();
  EXPECT_EQ(all.size(), 1u);
  EXPECT_TRUE(all.if_contains("os") != nullptr);
  EXPECT_TRUE(all.if_contains("docker") == nullptr) << "a frozen set from a module that is gone is worse than none";
}

TEST(FactRepository, ASetIsOwnedByOneModule) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  std::string error;
  EXPECT_EQ(repo.set("os", 2, parse(R"({"family":"windows"})"), error), set_result::rejected);
  EXPECT_NE(error.find("another module"), std::string::npos) << error;
  EXPECT_EQ(repo.get_all().at("os").as_object().at("family").as_string(), "linux");
  // Once the owner is gone the set is free again: this is what a module
  // reload looks like from here.
  repo.remove_owned_by(1);
  EXPECT_EQ(store(repo, "os", 2, R"({"family":"windows"})"), set_result::changed);
}

TEST(FactRepository, RetainOnlyDropsWhatTheProducerNoLongerMentions) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  store(repo, "hardware", 1, R"({"vendor":"Dell Inc."})");
  // `hardware` was turned off in the module's own configuration, so the next
  // round returns only `os`.
  repo.retain_only(1, {"os"});
  const boost::json::object all = repo.get_all();
  EXPECT_EQ(all.size(), 1u);
  EXPECT_TRUE(all.if_contains("os") != nullptr);
  EXPECT_EQ(repo.get_enabled(), std::set<std::string>{"os"});
}

TEST(FactRepository, RetainOnlyLeavesOtherProducersAlone) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  store(repo, "docker", 7, R"({"version":"26.1.0"})");
  repo.retain_only(1, {"os"});
  EXPECT_TRUE(repo.get_all().if_contains("docker") != nullptr) << "one module's round must not touch another module's sets";
  EXPECT_EQ(repo.get_enabled(), (std::set<std::string>{"os"}));
  repo.retain_only(7, {"docker"});
  EXPECT_EQ(repo.get_enabled(), (std::set<std::string>{"docker", "os"}));
}

TEST(FactRepository, AnIdInsideASetKeepsTheSet) {
  fact_repository repo;
  store(repo, "software", 1, R"({"installed":[{"id":"nscp"}]})");
  repo.retain_only(1, {"software.installed"});
  EXPECT_TRUE(repo.get_all().if_contains("software") != nullptr);
}

TEST(FactRepository, AProducerThatReturnsNothingLosesItsSets) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  repo.retain_only(1, std::set<std::string>());
  EXPECT_TRUE(repo.get_all().empty()) << "every set the module produced is now switched off in its configuration";
  EXPECT_EQ(repo.get_canonical(), "{}");
  EXPECT_TRUE(repo.get_enabled().empty());
}

TEST(FactRepository, RetainOnlyChangesNothingWhenTheSameSetsComeBack) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  const unsigned long long revision = repo.get_revision();
  repo.retain_only(1, {"os"});
  EXPECT_EQ(repo.get_revision(), revision);
}

TEST(FactRepository, UnloadingAModuleAlsoForgetsWhatItProduced) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  repo.retain_only(1, {"os"});
  repo.remove_owned_by(1);
  EXPECT_TRUE(repo.get_enabled().empty());
}

TEST(FactRepository, GetReadsADottedPath) {
  fact_repository repo;
  store(repo, "hardware", 1, R"({"cpu":{"model":"Xeon","cores":64}})");
  const boost::optional<boost::json::value> cores = repo.get("hardware.cpu.cores");
  ASSERT_TRUE(cores.is_initialized());
  EXPECT_EQ(cores.value().as_int64(), 64);
  EXPECT_FALSE(repo.get("hardware.cpu.speed").is_initialized());
  EXPECT_FALSE(repo.get("storage").is_initialized());
  EXPECT_FALSE(repo.get("").is_initialized());
  EXPECT_FALSE(repo.get("hardware.cpu.model.extra").is_initialized()) << "a path that walks into a scalar is not a subtree";
}

TEST(FactRepository, AnInvalidSetNameIsRejected) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(repo.set("OS", 1, parse(R"({"family":"linux"})"), error), set_result::rejected);
  EXPECT_EQ(repo.set("2fast", 1, parse(R"({"family":"linux"})"), error), set_result::rejected);
  EXPECT_EQ(repo.set("software.installed", 1, parse(R"({})"), error), set_result::rejected) << "a set is a top-level key, not a dotted settings id";
  EXPECT_TRUE(repo.get_all().empty());
}

TEST(FactRepository, ASetMustBeAnObject) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(repo.set("os", 1, parse(R"(["linux"])"), error), set_result::rejected);
  EXPECT_EQ(repo.set("os", 1, parse(R"("linux")"), error), set_result::rejected);
}

TEST(FactRepository, NullsAreRejected) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(repo.set("os", 1, parse(R"({"version":null})"), error), set_result::rejected);
  EXPECT_NE(error.find("os.version"), std::string::npos) << error;
}

TEST(FactRepository, KeysAreSnakeCase) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(repo.set("os", 1, parse(R"({"Family":"linux"})"), error), set_result::rejected);
  EXPECT_EQ(repo.set("os", 1, parse(R"({"boot time":"now"})"), error), set_result::rejected);
  EXPECT_EQ(repo.set("os", 1, parse(R"({"nested":{"BAD":1}})"), error), set_result::rejected);
  EXPECT_NE(error.find("os.nested.BAD"), std::string::npos) << error;
}

TEST(FactRepository, ARecordNeedsAnId) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(repo.set("storage", 1, parse(R"({"volumes":[{"fs":"ext4"}]})"), error), set_result::rejected);
  EXPECT_NE(error.find("id"), std::string::npos) << error;
  EXPECT_EQ(repo.set("storage", 1, parse(R"({"volumes":[{"id":"","fs":"ext4"}]})"), error), set_result::rejected);
}

TEST(FactRepository, RecordIdsAreUniqueWithinTheirList) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(repo.set("storage", 1, parse(R"({"volumes":[{"id":"c:"},{"id":"c:"}]})"), error), set_result::rejected);
  EXPECT_NE(error.find("two records"), std::string::npos) << error;
}

TEST(FactRepository, AListIsEitherRecordsOrStrings) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(store(repo, "network", 1, R"({"interfaces":[{"id":"eth0","addresses":["10.0.0.5","10.0.0.6"]}]})"), set_result::changed);
  EXPECT_EQ(repo.set("agent", 1, parse(R"({"modules":["CheckDisk",7]})"), error), set_result::rejected);
  EXPECT_EQ(repo.set("agent", 1, parse(R"({"modules":[{"id":"CheckDisk"},"CheckSystem"]})"), error), set_result::rejected);
}

TEST(FactRepository, DepthIsCapped) {
  fact_repository repo;
  std::string error;
  // The set itself is level 1, so six nested objects are the limit and a
  // seventh is one too many.
  EXPECT_EQ(store(repo, "deep", 1, R"({"a":{"b":{"c":{"d":{"e":{"f":1}}}}}})"), set_result::changed);
  EXPECT_EQ(repo.set("deeper", 1, parse(R"({"a":{"b":{"c":{"d":{"e":{"f":{"g":1}}}}}}})"), error), set_result::rejected);
  EXPECT_NE(error.find("deeper"), std::string::npos) << error;
}

TEST(FactRepository, TheSizeBudgetIsEnforcedAndKeepsThePreviousValue) {
  fact_repository repo;
  repo.set_max_size(256);
  store(repo, "os", 1, R"({"family":"linux"})");
  const std::string big(512, 'x');
  std::string error;
  EXPECT_EQ(repo.set("hardware", 1, parse("{\"model\":\"" + big + "\"}"), error), set_result::rejected);
  EXPECT_NE(error.find("budget"), std::string::npos) << error;
  const boost::json::object all = repo.get_all();
  EXPECT_EQ(all.size(), 1u);
  EXPECT_EQ(all.at("os").as_object().at("family").as_string(), "linux") << "a rejected set must not disturb what is already stored";
}

TEST(FactRepository, TheCanonicalFormSortsKeysAndDropsWhitespace) {
  fact_repository repo;
  store(repo, "os", 1, R"({ "version" : "6.1.0" , "family" : "linux" })");
  EXPECT_EQ(repo.get_canonical(), R"({"os":{"family":"linux","version":"6.1.0"}})");
}

TEST(FactRepository, TheHashIsStableAcrossInsertionOrder) {
  if (!fact_repository::can_hash()) GTEST_SKIP() << "built without OpenSSL: the document has no hash";
  fact_repository first;
  store(first, "os", 1, R"({"family":"linux"})");
  store(first, "hardware", 1, R"({"vendor":"Dell Inc."})");

  fact_repository second;
  store(second, "hardware", 1, R"({"vendor":"Dell Inc."})");
  store(second, "os", 1, R"({"family":"linux"})");

  EXPECT_EQ(first.get_hash(), second.get_hash()) << "the server compares hashes, so two hosts with the same inventory must agree";
}

TEST(FactRepository, TheEmptyDocumentHashIsThePinnedValue) {
  if (!fact_repository::can_hash()) GTEST_SKIP() << "built without OpenSSL: the document has no hash";
  const fact_repository repo;
  // sha256("{}"). Pinned because it is what a host with nothing enabled
  // reports in every state report, and the server reads it as "no inventory".
  EXPECT_EQ(repo.get_hash(), "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a");
}

TEST(FactRepository, ErrorsAreReplacedPerRound) {
  fact_repository repo;
  repo.set_errors({{"software.installed", "access denied"}});
  ASSERT_EQ(repo.get_errors().size(), 1u);
  EXPECT_EQ(repo.get_errors().at("software.installed"), "access denied");
  repo.set_errors(std::map<std::string, std::string>());
  EXPECT_TRUE(repo.get_errors().empty()) << "an error that was fixed must not linger";
}

TEST(FactRepository, CollectedIsWhatTheRoundRecorded) {
  fact_repository repo;
  repo.mark_collected("2026-09-19T14:03:11Z");
  EXPECT_EQ(repo.get_collected(), "2026-09-19T14:03:11Z");
}
