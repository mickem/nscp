// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "fact_repository.hpp"

#include <gtest/gtest.h>

#include <boost/json.hpp>

using fact_repository = nsclient::core::fact_repository;
using set_result = nsclient::core::fact_repository::set_result;

namespace {
// The document the repository stores is protobuf, but a test reads far better
// with the fact set written out as JSON than as twenty lines of message
// building. So the fixtures stay JSON and are converted here - through
// Boost.JSON rather than a hand-rolled parser, because a parser in the test is
// one more thing that can be wrong about what the test is asserting.
PB::Facts::Value convert(const boost::json::value &value) {
  PB::Facts::Value out;
  switch (value.kind()) {
    case boost::json::kind::object:
      for (const boost::json::key_value_pair &entry : value.get_object()) {
        PB::Facts::Field *field = out.mutable_object_value()->add_fields();
        field->set_key(std::string(entry.key()));
        *field->mutable_value() = convert(entry.value());
      }
      // An object with no members still has to say it is an object.
      out.mutable_object_value();
      return out;
    case boost::json::kind::array:
      for (const boost::json::value &item : value.get_array()) *out.mutable_list_value()->add_values() = convert(item);
      out.mutable_list_value();
      return out;
    case boost::json::kind::string:
      out.set_string_value(std::string(value.get_string().data(), value.get_string().size()));
      return out;
    case boost::json::kind::int64:
      out.set_int_value(value.get_int64());
      return out;
    case boost::json::kind::uint64:
      out.set_uint_value(value.get_uint64());
      return out;
    case boost::json::kind::double_:
      out.set_double_value(value.get_double());
      return out;
    case boost::json::kind::bool_:
      out.set_bool_value(value.get_bool());
      return out;
    default:
      // A null converts to a Value with nothing set, which is what the
      // repository rejects - see NullsAreRejected.
      return out;
  }
}

PB::Facts::Object parse(const std::string &json) { return convert(boost::json::parse(json)).object_value(); }

// Store a set and fail the test with the repository's own message if it was
// rejected - a rejection in a test that is not about rejection is a bug in
// the test data, and the message says which.
set_result store(fact_repository &repo, const std::string &name, const unsigned int plugin_id, const std::string &json) {
  std::string error;
  const set_result result = repo.set(name, plugin_id, parse(json), error);
  EXPECT_NE(result, set_result::rejected) << error;
  return result;
}

// The document as JSON, which is both what the fleet upload sends and the
// readable way to assert on a stored tree.
std::string json_of(const fact_repository &repo) { return repo.to_json(); }

bool has_set(const fact_repository &repo, const std::string &name) {
  const PB::Facts::Object all = repo.get_all();
  return nscapi::facts::tree::get(all, name) != nullptr;
}

std::size_t set_count(const fact_repository &repo) { return static_cast<std::size_t>(repo.get_all().fields_size()); }
}  // namespace

TEST(FactRepository, StartsEmptyAtRevisionZero) {
  const fact_repository repo;
  EXPECT_EQ(set_count(repo), 0u);
  EXPECT_EQ(repo.get_revision(), 0u);
  EXPECT_EQ(json_of(repo), "{}");
  EXPECT_TRUE(repo.get_errors().empty());
  EXPECT_TRUE(repo.get_collected().empty());
}

TEST(FactRepository, SetStoresAndBumpsRevision) {
  fact_repository repo;
  EXPECT_EQ(store(repo, "os", 1, R"({"family":"linux","version":"6.1.0"})"), set_result::changed);
  EXPECT_EQ(repo.get_revision(), 1u);
  EXPECT_EQ(set_count(repo), 1u);
  EXPECT_EQ(json_of(repo), R"({"os":{"family":"linux","version":"6.1.0"}})");
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
      << "the stored form is sorted, so key order alone is not a change";
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
  EXPECT_EQ(set_count(repo), 0u);
  EXPECT_EQ(repo.remove("docker"), set_result::unchanged);
}

TEST(FactRepository, UnloadingAModuleTakesItsSetsWithIt) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  store(repo, "docker", 7, R"({"version":"26.1.0"})");
  repo.remove_owned_by(7);
  EXPECT_EQ(set_count(repo), 1u);
  EXPECT_TRUE(has_set(repo, "os"));
  EXPECT_FALSE(has_set(repo, "docker")) << "a frozen set from a module that is gone is worse than none";
}

TEST(FactRepository, ASetIsOwnedByOneModule) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  std::string error;
  EXPECT_EQ(repo.set("os", 2, parse(R"({"family":"windows"})"), error), set_result::rejected);
  EXPECT_NE(error.find("another module"), std::string::npos) << error;
  EXPECT_EQ(json_of(repo), R"({"os":{"family":"linux"}})");
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
  EXPECT_EQ(set_count(repo), 1u);
  EXPECT_TRUE(has_set(repo, "os"));
  EXPECT_EQ(repo.get_enabled(), std::set<std::string>{"os"});
}

TEST(FactRepository, RetainOnlyLeavesOtherProducersAlone) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  store(repo, "docker", 7, R"({"version":"26.1.0"})");
  repo.retain_only(1, {"os"});
  EXPECT_TRUE(has_set(repo, "docker")) << "one module's round must not touch another module's sets";
  EXPECT_EQ(repo.get_enabled(), (std::set<std::string>{"os"}));
  repo.retain_only(7, {"docker"});
  EXPECT_EQ(repo.get_enabled(), (std::set<std::string>{"docker", "os"}));
}

TEST(FactRepository, AnIdInsideASetKeepsTheSet) {
  fact_repository repo;
  store(repo, "software", 1, R"({"installed":[{"id":"nscp"}]})");
  repo.retain_only(1, {"software.installed"});
  EXPECT_TRUE(has_set(repo, "software"));
}

TEST(FactRepository, AProducerThatReturnsNothingLosesItsSets) {
  fact_repository repo;
  store(repo, "os", 1, R"({"family":"linux"})");
  repo.retain_only(1, std::set<std::string>());
  EXPECT_EQ(set_count(repo), 0u) << "every set the module produced is now switched off in its configuration";
  EXPECT_EQ(json_of(repo), "{}");
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
  const boost::optional<PB::Facts::Value> cores = repo.get("hardware.cpu.cores");
  ASSERT_TRUE(cores.is_initialized());
  EXPECT_EQ(cores.value().int_value(), 64);
  const boost::optional<PB::Facts::Value> cpu = repo.get("hardware.cpu");
  ASSERT_TRUE(cpu.is_initialized());
  EXPECT_EQ(nscapi::facts::tree::to_json(cpu.value()), R"({"cores":64,"model":"Xeon"})");
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
  EXPECT_EQ(set_count(repo), 0u);
}

TEST(FactRepository, NullsAreRejected) {
  fact_repository repo;
  std::string error;
  // A Value with no member set is the protobuf shape of a null, and what a
  // hand-built message carries when a producer forgets to fill a field in.
  EXPECT_EQ(repo.set("os", 1, parse(R"({"version":null})"), error), set_result::rejected);
  EXPECT_NE(error.find("os.version"), std::string::npos) << error;
}

TEST(FactRepository, TheSameKeyTwiceIsRejected) {
  fact_repository repo;
  PB::Facts::Object twice;
  PB::Facts::Field *first = twice.add_fields();
  first->set_key("family");
  first->mutable_value()->set_string_value("linux");
  PB::Facts::Field *second = twice.add_fields();
  second->set_key("family");
  second->mutable_value()->set_string_value("windows");
  std::string error;
  // The repeated field can carry it; an object cannot mean it. Which one wins
  // would then depend on the reader, which is not a document to build on.
  EXPECT_EQ(repo.set("os", 1, twice, error), set_result::rejected);
  EXPECT_NE(error.find("twice"), std::string::npos) << error;
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
  EXPECT_EQ(json_of(repo), R"({"os":{"family":"linux"}})") << "a rejected set must not disturb what is already stored";
}

TEST(FactRepository, TheJsonRenderingSortsKeysAndDropsWhitespace) {
  fact_repository repo;
  store(repo, "os", 1, R"({ "version" : "6.1.0" , "family" : "linux" })");
  store(repo, "agent", 1, R"({"modules":["CheckDisk","CheckSystem"]})");
  // What the fleet upload sends: sets in order, keys sorted, no whitespace -
  // and lists left in the order the producer reported them.
  EXPECT_EQ(json_of(repo), R"({"agent":{"modules":["CheckDisk","CheckSystem"]},"os":{"family":"linux","version":"6.1.0"}})");
}

TEST(FactRepository, TheJsonRenderingCarriesEveryScalarType) {
  fact_repository repo;
  PB::Facts::Object set;
  PB::Facts::Field *text = set.add_fields();
  text->set_key("text");
  text->mutable_value()->set_string_value("a \"quoted\"\tvalue");
  PB::Facts::Field *count = set.add_fields();
  count->set_key("count");
  count->mutable_value()->set_int_value(-7);
  PB::Facts::Field *size = set.add_fields();
  size->set_key("size");
  size->mutable_value()->set_uint_value(18446744073709551615ull);
  PB::Facts::Field *load = set.add_fields();
  load->set_key("load");
  load->mutable_value()->set_double_value(1.5);
  PB::Facts::Field *on = set.add_fields();
  on->set_key("on");
  on->mutable_value()->set_bool_value(true);
  std::string error;
  ASSERT_EQ(repo.set("mixed", 1, set, error), set_result::changed) << error;
  // A uint64 past int64 survives as itself, a double keeps its decimal point
  // whatever the host's locale says, and a string is escaped. Held in a
  // variable because MSVC's preprocessor does not keep a raw string carrying
  // an escaped quote in one piece when it is a macro argument.
  const std::string expected = R"({"mixed":{"count":-7,"load":1.5,"on":true,"size":18446744073709551615,"text":"a \"quoted\"\tvalue"}})";
  EXPECT_EQ(json_of(repo), expected);
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
  // The hash is taken of the JSON rendering, not of the stored protobuf:
  // protobuf defines no canonical encoding, and this number has to mean the
  // same thing to an implementation that is not this one.
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

// The age of a set's values, kept beside the set rather than in it.
//
// It has to live outside the document because set() decides "did anything
// change" by comparing encoded bytes: a timestamp inside would make every
// round a change, bump the revision hourly forever, and re-upload an
// inventory that never moved.

TEST(FactRepository, RecordsWhenASetsValuesWereGathered) {
  fact_repository repo;
  ASSERT_EQ(store(repo, "os", 1, R"({"family":"windows"})"), set_result::changed);
  repo.mark_gathered("os", "2026-09-23T08:00:00Z");
  ASSERT_EQ(repo.get_gathered().count("os"), 1u);
  EXPECT_EQ(repo.get_gathered().at("os"), "2026-09-23T08:00:00Z");
}

TEST(FactRepository, AnUnchangedRoundStillRecordsTheAge) {
  // The case this exists for: a producer that caches hands back the same
  // bytes every round, so set() stops at `unchanged` without touching
  // anything - but the producer still has something true to say about when
  // it read them.
  fact_repository repo;
  ASSERT_EQ(store(repo, "os", 1, R"({"family":"windows"})"), set_result::changed);
  repo.mark_gathered("os", "2026-09-23T08:00:00Z");
  const unsigned long long revision = repo.get_revision();

  ASSERT_EQ(store(repo, "os", 1, R"({"family":"windows"})"), set_result::unchanged);
  repo.mark_gathered("os", "2026-09-23T09:00:00Z");

  EXPECT_EQ(repo.get_gathered().at("os"), "2026-09-23T09:00:00Z");
  EXPECT_EQ(repo.get_revision(), revision) << "recording the age must not look like a change to the document";
}

TEST(FactRepository, AGatheredTimeForASetWeDoNotHoldIsIgnored) {
  // Otherwise a rejected or never-stored set would leave a timestamp behind
  // describing data that is not there.
  fact_repository repo;
  repo.mark_gathered("storage", "2026-09-23T08:00:00Z");
  EXPECT_TRUE(repo.get_gathered().empty());
}

TEST(FactRepository, RemovingASetTakesItsGatheredTimeWithIt) {
  fact_repository repo;
  ASSERT_EQ(store(repo, "os", 1, R"({"family":"windows"})"), set_result::changed);
  repo.mark_gathered("os", "2026-09-23T08:00:00Z");
  ASSERT_EQ(repo.remove("os"), set_result::changed);
  EXPECT_TRUE(repo.get_gathered().empty());
}

TEST(FactRepository, AnEmptyGatheredTimeClearsIt) {
  // A producer that stops saying falls back to "the time of the round", which
  // is what the core reports when there is no per-set time.
  fact_repository repo;
  ASSERT_EQ(store(repo, "os", 1, R"({"family":"windows"})"), set_result::changed);
  repo.mark_gathered("os", "2026-09-23T08:00:00Z");
  repo.mark_gathered("os", "");
  EXPECT_TRUE(repo.get_gathered().empty());
}
