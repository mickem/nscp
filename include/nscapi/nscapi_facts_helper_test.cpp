// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>

using namespace nscapi::facts;

namespace {
using nscapi::facts::testing::error_of;
using nscapi::facts::testing::find_set;
using nscapi::facts::testing::json_of;

std::string serialized_request(const std::string &reason) {
  PB::Facts::FactsQueryMessage message;
  message.add_payload()->set_reason(reason);
  return message.SerializeAsString();
}
}  // namespace

TEST(FactsRequest, ReadsTheReason) {
  const request req(serialized_request("startup"));
  EXPECT_EQ(req.reason(), "startup");
}

TEST(FactsRequest, AnUnreadableRequestHasNoReason) {
  const request req(std::string("\xff\xff\xff\xff not a request", 19));
  EXPECT_TRUE(req.reason().empty()) << "a producer that cannot read the reason collects as on a scheduled round";
}

TEST(FactsResponse, AnEmptyResponseHasNoSets) {
  const response out;
  const PB::Facts::FactsMessage message = out.to_message();
  ASSERT_EQ(message.payload_size(), 1);
  EXPECT_EQ(message.payload(0).sets_size(), 0);
  EXPECT_EQ(message.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_OK) << "producing nothing is not a failure";
}

TEST(FactsResponse, BuildsASetOfScalars) {
  response out;
  section os = out.set("os");
  os.value("family", "linux").value("version", "6.1.0").value("cores", 64).value("virtual", true);
  EXPECT_EQ(json_of(out, "os"), R"({"family":"linux","version":"6.1.0","cores":64,"virtual":true})");
}

TEST(FactsResponse, AnEmptyStringIsNotAValue) {
  response out;
  out.set("os").value("family", "linux").value("build", "").value("kernel", static_cast<const char *>(nullptr));
  EXPECT_EQ(json_of(out, "os"), R"({"family":"linux"})") << "an unknown value is omitted, never written as an empty string";
}

TEST(FactsResponse, HandlesStayValidWhileOtherSetsAreAdded) {
  response out;
  section os = out.set("os");
  out.set("hardware").value("vendor", "Dell Inc.");
  // The producer keeps filling the first set after starting the second; the
  // handle must still point at the first.
  os.value("family", "linux");
  EXPECT_EQ(json_of(out, "os"), R"({"family":"linux"})");
  EXPECT_EQ(json_of(out, "hardware"), R"({"vendor":"Dell Inc."})");
}

TEST(FactsResponse, TheSameSetTwiceIsTheSameSection) {
  response out;
  out.set("software").list("installed").record("nscp").value("version", "0.20.0");
  out.set("software").list("hotfixes").record("KB1");
  EXPECT_EQ(json_of(out, "software"), R"({"installed":[{"id":"nscp","version":"0.20.0"}],"hotfixes":[{"id":"KB1"}]})");
}

TEST(FactsResponse, RecordsCarryTheirId) {
  response out;
  record_list volumes = out.set("storage").list("volumes");
  volumes.record("c:").value("fs", "NTFS").value("size_bytes", 255000000000LL);
  volumes.record("d:").value("fs", "NTFS");
  EXPECT_EQ(json_of(out, "storage"), R"({"volumes":[{"id":"c:","fs":"NTFS","size_bytes":255000000000},{"id":"d:","fs":"NTFS"}]})");
}

TEST(FactsResponse, ARecordWithoutAnIdIsReported) {
  response out;
  out.set("storage").list("volumes").record("").value("fs", "NTFS");
  EXPECT_NE(error_of(out, "storage").find("without an id"), std::string::npos) << error_of(out, "storage");
}

TEST(FactsResponse, NestedObjectsAndStringListsWork) {
  response out;
  section hardware = out.set("hardware");
  hardware.object("cpu").value("model", "Xeon").value("cores", 64);
  out.set("network").list("interfaces").record("eth0").strings("addresses", {"10.0.0.5", "", "10.0.0.6"});
  EXPECT_EQ(json_of(out, "hardware"), R"({"cpu":{"model":"Xeon","cores":64}})");
  EXPECT_EQ(json_of(out, "network"), R"({"interfaces":[{"id":"eth0","addresses":["10.0.0.5","10.0.0.6"]}]})") << "an empty string is skipped in a list too";
}

TEST(FactsResponse, NumbersKeepTheirSignedness) {
  response out;
  // A size that does not fit in an int64 has to survive as itself: a disk or
  // a memory total on a large host is exactly where this shows up.
  out.set("hardware").value("memory_bytes", 18446744073709551615ull).value("offset", -3).value("load", 1.5);
  EXPECT_EQ(json_of(out, "hardware"), R"({"memory_bytes":18446744073709551615,"offset":-3,"load":1.5})");
}

TEST(FactsResponse, TimestampsAreFormattedTheOneAllowedWay) {
  response out;
  // 2026-09-19T14:03:11Z
  out.set("os").time("boot_time", 1789826591).date("installed", 1789826591).time("never", 0);
  EXPECT_EQ(json_of(out, "os"), R"({"boot_time":"2026-09-19T14:03:11Z","installed":"2026-09-19"})") << "an unknown time is omitted, like an unknown string";
}

TEST(FactsResponse, AnInvalidKeyIsDroppedAndReported) {
  response out;
  out.set("os").value("family", "linux").value("Boot Time", "now");
  EXPECT_EQ(json_of(out, "os"), R"({"family":"linux"})") << "the rest of the set still ships";
  EXPECT_NE(error_of(out, "os").find("Boot Time"), std::string::npos) << error_of(out, "os");
}

TEST(FactsResponse, RemoveIsExplicit) {
  response out;
  out.set("docker").value("version", "26.1.0");
  out.remove("docker");
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *docker = find_set(message, "docker");
  ASSERT_TRUE(docker != nullptr);
  EXPECT_TRUE(docker->removed()) << "not returning a set keeps the old value; removing it drops it";
  EXPECT_FALSE(docker->has_facts());
}

TEST(FactsResponse, ErrorsAreReportedPerSet) {
  response out;
  out.error("software.installed", "access denied to HKLM");
  EXPECT_EQ(error_of(out, "software.installed"), "access denied to HKLM");
  // The set rides along even though nothing was built for it, so the core
  // learns this module produces it and keeps the value it holds.
  EXPECT_EQ(json_of(out, "software.installed"), "(no facts)");
}

TEST(FactsResponse, AFailedRoundSaysSoInTheResult) {
  response out;
  out.set("os").value("family", "linux");
  out.failed("Failed to collect facts: the collector threw");
  const PB::Facts::FactsMessage message = out.to_message();
  ASSERT_EQ(message.payload_size(), 1);
  EXPECT_EQ(message.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  EXPECT_NE(message.payload(0).result().message().find("the collector threw"), std::string::npos);
  EXPECT_EQ(message.payload(0).sets_size(), 0) << "a failed round carries nothing, so the core keeps what it has";
}

TEST(FactsHelper, ValidatesKeysTheWayTheCoreDoes) {
  EXPECT_TRUE(is_valid_key("boot_time"));
  EXPECT_TRUE(is_valid_key("total_bytes2"));
  EXPECT_FALSE(is_valid_key(""));
  EXPECT_FALSE(is_valid_key("Boot"));
  EXPECT_FALSE(is_valid_key("boot time"));
  EXPECT_FALSE(is_valid_key("2fast"));
  EXPECT_FALSE(is_valid_key("_leading"));
  EXPECT_FALSE(is_valid_key(std::string(65, 'k')));
  EXPECT_TRUE(is_valid_key(std::string(64, 'k')));
}

TEST(FactsHelper, ParseDocumentReadsTheEnvelope) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = message.add_payload();
  payload->set_revision(3);
  PB::Facts::Field *os = payload->mutable_facts()->mutable_object_value()->add_fields();
  os->set_key("os");
  PB::Facts::Field *family = os->mutable_value()->mutable_object_value()->add_fields();
  family->set_key("family");
  family->mutable_value()->set_string_value("linux");
  EXPECT_EQ(tree::to_json(parse_document(message.SerializeAsString())), R"({"os":{"family":"linux"}})");

  // A core without the facts API answers with nothing, and a subtree that is
  // not there has no facts member at all.
  EXPECT_EQ(tree::to_json(parse_document("")), "{}");
  PB::Facts::FactsResponseMessage missing;
  missing.add_payload()->set_found(false);
  EXPECT_EQ(tree::to_json(parse_document(missing.SerializeAsString())), "{}");
  EXPECT_EQ(tree::to_json(parse_document(std::string("\xff\xff\xff\xff junk", 10))), "{}");
}

TEST(FactsTree, SortFieldsIsTheCanonicalOrder) {
  response out;
  out.set("os").value("version", "6.1.0").value("family", "linux").object("kernel").value("release", "6.1.0").value("arch", "x86_64");
  const PB::Facts::FactsMessage message = out.to_message();
  ASSERT_EQ(message.payload(0).sets_size(), 1);
  PB::Facts::Object os = message.payload(0).sets(0).facts();
  tree::sort_fields(&os);
  // Every level is sorted, not just the top one - that is what makes two
  // agents with the same inventory produce the same bytes and the same hash.
  EXPECT_EQ(tree::to_json(os), R"({"family":"linux","kernel":{"arch":"x86_64","release":"6.1.0"},"version":"6.1.0"})");
}

TEST(FactsTree, ListsKeepTheOrderTheProducerReported) {
  response out;
  out.set("agent").strings("modules", {"CheckSystem", "CheckDisk"});
  PB::Facts::Object agent = out.to_message().payload(0).sets(0).facts();
  tree::sort_fields(&agent);
  EXPECT_EQ(tree::to_json(agent), R"({"modules":["CheckSystem","CheckDisk"]})") << "a list is ordered data, so sorting it would change the document";
}
