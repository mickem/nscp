// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <string>

#include "fact_repository.hpp"

using nsclient::core::fact_repository;
using set_result = nsclient::core::fact_repository::set_result;

namespace {
boost::json::value parse(const std::string &text) { return boost::json::parse(text); }

// set() with the error out-parameter swallowed, for the arrange half of a test
// that is asserting on something else.
set_result store(fact_repository &repo, const std::string &id, const std::string &json, const unsigned int plugin_id = 1) {
  std::string error;
  return repo.set(id, plugin_id, parse(json), error);
}
}  // namespace

// ============================================================================
// The empty repository
// ============================================================================

TEST(FactRepository, StartsEmptyAtRevisionZero) {
  const fact_repository repo;
  EXPECT_TRUE(repo.get_all().empty());
  EXPECT_EQ(0u, repo.get_revision());
  EXPECT_TRUE(repo.get_errors().empty());
}

TEST(FactRepository, TheEmptyDocumentIsAnEmptyObjectNotNothing) {
  const fact_repository repo;
  EXPECT_EQ("{}", repo.get_document());
}

// A host with no fact set enabled still reports a hash; the server uses it to
// tell "inventory switched off" from "agent too old to have any".
TEST(FactRepository, TheEmptyDocumentHashesToTheDigestOfTwoBraces) {
  const fact_repository repo;
  EXPECT_EQ(algorithms::sha256_hex("{}"), repo.get_hash());
}

// ============================================================================
// Storing sets
// ============================================================================

TEST(FactRepository, SetStoresTheValueAndBumpsTheRevision) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::changed, repo.set("os", 1, parse(R"({"family":"linux"})"), error));
  EXPECT_EQ("", error);
  EXPECT_EQ(1u, repo.get_revision());
  EXPECT_EQ(R"({"os":{"family":"linux"}})", repo.get_document());
}

TEST(FactRepository, RepublishingTheSameValueIsANoOp) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(set_result::unchanged, store(repo, "os", R"({"family":"linux"})"));
  EXPECT_EQ(revision, repo.get_revision()) << "revision must only move on a real change";
}

TEST(FactRepository, ChangingAValueBumpsTheRevision) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(set_result::changed, store(repo, "os", R"({"family":"windows"})"));
  EXPECT_EQ(revision + 1, repo.get_revision());
}

TEST(FactRepository, RemovingASetBumpsTheRevisionAndRemovingAnAbsentOneDoesNot) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  const unsigned long long revision = repo.get_revision();
  EXPECT_EQ(set_result::changed, repo.remove("os"));
  EXPECT_EQ(revision + 1, repo.get_revision());
  EXPECT_EQ(set_result::unchanged, repo.remove("os"));
  EXPECT_EQ(revision + 1, repo.get_revision());
}

// A depth-2 id is how one module splits enablement finer than its top-level
// key; two of them have to become one object in the document.
TEST(FactRepository, TwoDottedSetsShareOneTopLevelKeyInTheDocument) {
  fact_repository repo;
  store(repo, "software.installed", R"([{"id":"curl","version":"8.5.0"}])");
  store(repo, "software.hotfixes", R"([{"id":"KB5000001"}])");
  EXPECT_EQ(R"({"software":{"hotfixes":[{"id":"KB5000001"}],"installed":[{"id":"curl","version":"8.5.0"}]}})", repo.get_document());
}

TEST(FactRepository, RemovingOneDottedSetLeavesItsSibling) {
  fact_repository repo;
  store(repo, "software.installed", R"([{"id":"curl"}])");
  store(repo, "software.hotfixes", R"([{"id":"KB5000001"}])");
  repo.remove("software.hotfixes");
  EXPECT_EQ(R"({"software":{"installed":[{"id":"curl"}]}})", repo.get_document());
}

TEST(FactRepository, GetSetIdsNamesWhatIsStoredNotWhatIsEnabled) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  store(repo, "software.installed", R"([{"id":"curl"}])");
  const std::set<std::string> ids = repo.get_set_ids();
  EXPECT_EQ(2u, ids.size());
  EXPECT_EQ(1u, ids.count("os"));
  EXPECT_EQ(1u, ids.count("software.installed"));
}

// ============================================================================
// Document rules
// ============================================================================

TEST(FactRepository, ARejectedSetKeepsThePreviousValue) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, parse(R"({"Family":"linux"})"), error));
  EXPECT_FALSE(error.empty()) << "a rejection must say why";
  EXPECT_EQ(R"({"os":{"family":"linux"}})", repo.get_document()) << "the previous value must survive the rejection";
}

TEST(FactRepository, AKeyThatIsNotSnakeCaseIsRejected) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, parse(R"({"Family":"linux"})"), error));
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, parse(R"({"family-name":"linux"})"), error));
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, parse(R"({"1st":"linux"})"), error));
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, parse(R"({"":"linux"})"), error));
}

TEST(FactRepository, AKeyPastSixtyFourCharactersIsRejected) {
  fact_repository repo;
  std::string error;
  const std::string key(65, 'a');
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, parse("{\"" + key + "\":1}"), error));
  EXPECT_EQ(set_result::changed, repo.set("os", 1, parse("{\"" + key.substr(1) + "\":1}"), error));
}

TEST(FactRepository, ANullAnywhereIsRejectedBecauseAnUnknownValueIsOmitted) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, parse(R"({"family":null})"), error));
  EXPECT_EQ(set_result::rejected, repo.set("os", 1, boost::json::value(), error));
}

TEST(FactRepository, NestingDeeperThanSixLevelsIsRejected) {
  fact_repository repo;
  std::string error;
  // `os` is level 1, so five more objects reach level 6 and six reach 7.
  EXPECT_EQ(set_result::changed, repo.set("os", 1, parse(R"({"a":{"b":{"c":{"d":{"e":1}}}}})"), error));
  EXPECT_EQ(set_result::rejected, repo.set("deep", 1, parse(R"({"a":{"b":{"c":{"d":{"e":{"f":1}}}}}})"), error));
}

TEST(FactRepository, ADottedSetsOwnDepthCountsAgainstTheBudget) {
  fact_repository repo;
  std::string error;
  // `software.installed` is already two levels, so it gets four, not five.
  EXPECT_EQ(set_result::rejected, repo.set("software.installed", 1, parse(R"({"a":{"b":{"c":{"d":{"e":1}}}}})"), error));
}

TEST(FactRepository, ARecordWithoutAnIdIsRejectedBecauseTheServerDiffsListsById) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("storage.volumes", 1, parse(R"([{"fs":"ext4"}])"), error));
  EXPECT_EQ(set_result::rejected, repo.set("storage.volumes", 1, parse(R"([{"id":"","fs":"ext4"}])"), error));
  EXPECT_EQ(set_result::rejected, repo.set("storage.volumes", 1, parse(R"([{"id":3,"fs":"ext4"}])"), error));
}

TEST(FactRepository, TwoRecordsWithTheSameIdAreRejected) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("storage.volumes", 1, parse(R"([{"id":"/"},{"id":"/"}])"), error));
}

TEST(FactRepository, APlainListOfStringsIsAllowed) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::changed, repo.set("agent", 1, parse(R"({"modules":["CheckDisk","CheckSystem"]})"), error));
}

TEST(FactRepository, AListMixingRecordsWithPlainValuesIsRejected) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("storage.volumes", 1, parse(R"([{"id":"/"},"also /"])"), error));
  EXPECT_EQ(set_result::rejected, repo.set("agent", 1, parse(R"({"modules":["CheckDisk",7]})"), error));
}

TEST(FactRepository, AListPastFiveThousandRecordsIsRejected) {
  fact_repository repo;
  boost::json::array list;
  for (int i = 0; i < 5001; ++i) {
    boost::json::object record;
    record["id"] = "pkg" + std::to_string(i);
    list.push_back(record);
  }
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("software.installed", 1, boost::json::value(list), error));
  list.erase(list.begin());
  EXPECT_EQ(set_result::changed, repo.set("software.installed", 1, boost::json::value(list), error));
}

TEST(FactRepository, AnIdWithMoreThanTwoComponentsIsNotAFactSet) {
  fact_repository repo;
  std::string error;
  EXPECT_EQ(set_result::rejected, repo.set("software.installed.packages", 1, parse("{}"), error));
  EXPECT_FALSE(fact_repository::is_valid_id("software.installed.packages"));
  EXPECT_FALSE(fact_repository::is_valid_id("Software"));
  EXPECT_FALSE(fact_repository::is_valid_id(""));
  EXPECT_TRUE(fact_repository::is_valid_id("os"));
  EXPECT_TRUE(fact_repository::is_valid_id("software.installed"));
}

TEST(FactRepository, ASetThatWouldPushTheDocumentPastTheSizeCapIsRejected) {
  fact_repository repo;
  repo.set_max_size(256);
  std::string error;
  EXPECT_EQ(set_result::changed, repo.set("os", 1, parse(R"({"family":"linux"})"), error));
  const boost::json::value big = parse("{\"model\":\"" + std::string(300, 'x') + "\"}");
  EXPECT_EQ(set_result::rejected, repo.set("hardware", 1, big, error));
  EXPECT_NE(std::string::npos, error.find("256")) << "the rejection must name the limit: " << error;
  EXPECT_EQ(R"({"os":{"family":"linux"}})", repo.get_document());
}

// A producer that shrinks an oversized set must be able to get back under the
// cap; budgeting the replacement on top of what it replaces would wedge it.
TEST(FactRepository, ReplacingASetIsBudgetedAgainstWhatItReplaces) {
  fact_repository repo;
  repo.set_max_size(400);
  std::string error;
  ASSERT_EQ(set_result::changed, repo.set("hardware", 1, parse("{\"model\":\"" + std::string(300, 'x') + "\"}"), error));
  EXPECT_EQ(set_result::changed, repo.set("hardware", 1, parse("{\"model\":\"" + std::string(290, 'y') + "\"}"), error)) << error;
}

// ============================================================================
// Canonical serialisation and the hash
// ============================================================================

TEST(FactRepository, TheCanonicalFormSortsObjectKeysAndDropsWhitespace) {
  EXPECT_EQ(R"({"a":1,"b":2,"z":3})", fact_repository::canonical(parse(R"({ "z": 3, "a": 1, "b": 2 })")));
  EXPECT_EQ(R"({"a":{"x":1,"y":2}})", fact_repository::canonical(parse(R"({"a":{"y":2,"x":1}})")));
}

// List order belongs to the producer - it is the order the check that owns the
// instance enumerates - so it must survive canonicalisation untouched.
TEST(FactRepository, TheCanonicalFormKeepsListOrder) {
  EXPECT_EQ(R"([{"id":"d:"},{"id":"c:"}])", fact_repository::canonical(parse(R"([{"id":"d:"},{"id":"c:"}])")));
}

TEST(FactRepository, TheCanonicalFormPinsTheBytesOfAFixedDocument) {
  fact_repository repo;
  std::string error;
  ASSERT_EQ(set_result::changed, repo.set("os", 1, parse(R"({"version":"6.8.0","family":"linux","boot_time":"2026-09-01T04:12:09Z"})"), error));
  ASSERT_EQ(set_result::changed, repo.set("storage.volumes", 2, parse(R"([{"size_bytes":255000000000,"id":"/","fs":"ext4"}])"), error));
  const std::string expected =
      R"({"os":{"boot_time":"2026-09-01T04:12:09Z","family":"linux","version":"6.8.0"},)"
      R"("storage":{"volumes":[{"fs":"ext4","id":"/","size_bytes":255000000000}]}})";
  EXPECT_EQ(expected, repo.get_document());
  EXPECT_EQ(algorithms::sha256_hex(expected), repo.get_hash());
}

// Two agents that collected the same inventory must report the same hash even
// if their producers happened to emit the keys in a different order.
TEST(FactRepository, TheHashIsIndependentOfTheOrderKeysWerePublishedIn) {
  fact_repository first;
  store(first, "os", R"({"family":"linux","version":"6.8.0"})");
  store(first, "hardware", R"({"vendor":"Dell Inc."})");
  fact_repository second;
  store(second, "hardware", R"({"vendor":"Dell Inc."})");
  store(second, "os", R"({"version":"6.8.0","family":"linux"})");
  EXPECT_EQ(first.get_hash(), second.get_hash());
}

TEST(FactRepository, TheHashMovesWhenTheDocumentChangesAndIsStableWhenItDoesNot) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  const std::string before = repo.get_hash();
  EXPECT_EQ(before, repo.get_hash()) << "a second read must not recompute a different digest";
  store(repo, "os", R"({"family":"windows"})");
  EXPECT_NE(before, repo.get_hash());
}

TEST(FactRepository, RemovingEveryFactSetReturnsTheEmptyDocumentHash) {
  fact_repository repo;
  const std::string empty = repo.get_hash();
  store(repo, "os", R"({"family":"linux"})");
  repo.remove("os");
  EXPECT_EQ(empty, repo.get_hash());
}

// ============================================================================
// Lookups
// ============================================================================

TEST(FactRepository, GetReturnsTheSubtreeAtADottedPath) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux","version":"6.8.0"})");
  store(repo, "software.installed", R"([{"id":"curl","version":"8.5.0"}])");

  const auto os = repo.get("os");
  ASSERT_TRUE(os.is_initialized());
  EXPECT_EQ(R"({"family":"linux","version":"6.8.0"})", fact_repository::canonical(os.value()));

  const auto installed = repo.get("software.installed");
  ASSERT_TRUE(installed.is_initialized());
  EXPECT_TRUE(installed.value().is_array());

  const auto family = repo.get("os.family");
  ASSERT_TRUE(family.is_initialized());
  EXPECT_EQ(R"("linux")", fact_repository::canonical(family.value()));
}

TEST(FactRepository, GetReturnsNothingForAPathThatIsNotThere) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  EXPECT_FALSE(repo.get("hardware").is_initialized());
  EXPECT_FALSE(repo.get("os.arch").is_initialized());
  EXPECT_FALSE(repo.get("os.family.deeper").is_initialized());
}

TEST(FactRepository, GetWithAnEmptyPathReturnsTheWholeDocument) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  const auto all = repo.get("");
  ASSERT_TRUE(all.is_initialized());
  EXPECT_EQ(R"({"os":{"family":"linux"}})", fact_repository::canonical(all.value()));
}

// ============================================================================
// Ownership and enablement
// ============================================================================

TEST(FactRepository, RemoveOwnedByDropsOnlyThatPluginsSets) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})", 7);
  store(repo, "docker", R"({"version":"26.0"})", 9);
  const unsigned long long revision = repo.get_revision();
  repo.remove_owned_by(9);
  EXPECT_EQ(revision + 1, repo.get_revision());
  EXPECT_EQ(R"({"os":{"family":"linux"}})", repo.get_document());
}

TEST(FactRepository, RemoveOwnedByForAPluginThatPublishedNothingDoesNotBumpTheRevision) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})", 7);
  const unsigned long long revision = repo.get_revision();
  repo.remove_owned_by(9);
  EXPECT_EQ(revision, repo.get_revision());
}

// A module that reloads keeps its sets, but under its new plugin id, so the
// unload that follows a later reload still finds them.
TEST(FactRepository, RepublishingAnUnchangedSetStillRefreshesItsOwner) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})", 7);
  ASSERT_EQ(set_result::unchanged, store(repo, "os", R"({"family":"linux"})", 8));
  repo.remove_owned_by(8);
  EXPECT_EQ("{}", repo.get_document());
}

TEST(FactRepository, RetainOnlyDropsSetsThatAreNoLongerEnabled) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  store(repo, "hardware", R"({"vendor":"Dell Inc."})");
  store(repo, "software.installed", R"([{"id":"curl"}])");
  const unsigned long long revision = repo.get_revision();

  std::set<std::string> enabled;
  enabled.insert("os");
  repo.retain_only(enabled);

  EXPECT_EQ(revision + 1, repo.get_revision());
  EXPECT_EQ(R"({"os":{"family":"linux"}})", repo.get_document());
}

TEST(FactRepository, RetainOnlyWithNothingToDropDoesNotBumpTheRevision) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  const unsigned long long revision = repo.get_revision();
  std::set<std::string> enabled;
  enabled.insert("os");
  repo.retain_only(enabled);
  EXPECT_EQ(revision, repo.get_revision());
}

// ============================================================================
// Collection errors
// ============================================================================

TEST(FactRepository, AnErrorIsKeptPerSetAndClearedWhenItPasses) {
  fact_repository repo;
  repo.set_error("software.installed", 3, "access denied to HKLM\\...");
  EXPECT_EQ(1u, repo.get_errors().size());
  EXPECT_EQ("access denied to HKLM\\...", repo.get_errors().at("software.installed"));
  repo.clear_error("software.installed");
  EXPECT_TRUE(repo.get_errors().empty());
}

TEST(FactRepository, AnErrorDoesNotMoveTheRevisionBecauseTheDocumentDidNotChange) {
  fact_repository repo;
  store(repo, "os", R"({"family":"linux"})");
  const unsigned long long revision = repo.get_revision();
  repo.set_error("hardware", 1, "dmi not readable");
  EXPECT_EQ(revision, repo.get_revision());
  EXPECT_EQ(R"({"os":{"family":"linux"}})", repo.get_document());
}

TEST(FactRepository, ErrorsGoAwayWithTheirPluginAndWithTheirEnablement) {
  fact_repository repo;
  repo.set_error("docker", 9, "socket missing");
  repo.set_error("hardware", 7, "dmi not readable");
  repo.remove_owned_by(9);
  EXPECT_EQ(1u, repo.get_errors().size());

  std::set<std::string> enabled;
  repo.retain_only(enabled);
  EXPECT_TRUE(repo.get_errors().empty());
}
