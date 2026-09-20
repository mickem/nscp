// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <boost/json.hpp>
#include <nscapi/nscapi_facts_helper.hpp>

using namespace nscapi::facts;

namespace {
boost::json::value document_of(const response &out) { return boost::json::parse(out.to_json()); }

const boost::json::object &sets_of(const boost::json::value &document) { return document.as_object().at("sets").as_object(); }

std::string request_for(const std::string &ids, const std::string &reason = "scheduled") { return "{\"enabled\":[" + ids + "],\"reason\":\"" + reason + "\"}"; }
}  // namespace

TEST(FactsRequest, ReadsTheEnabledIdsAndTheReason) {
  const request req(request_for("\"os\",\"software.installed\"", "startup"));
  EXPECT_EQ(req.reason(), "startup");
  EXPECT_TRUE(req.wants("os"));
  EXPECT_TRUE(req.wants("software.installed"));
  EXPECT_FALSE(req.wants("hardware"));
  EXPECT_FALSE(req.wants("software.hotfixes"));
}

TEST(FactsRequest, ASetIsWantedWhenOneOfItsPartsIs) {
  const request req(request_for("\"software.installed\""));
  EXPECT_TRUE(req.wants("software")) << "a producer gates the set first and then decides per part";
  EXPECT_FALSE(req.wants("softwareother"));
}

TEST(FactsRequest, AnUnparseableRequestAsksForNothing) {
  const request req("not json");
  EXPECT_TRUE(req.enabled().empty());
  EXPECT_FALSE(req.wants("os"));
  EXPECT_TRUE(req.reason().empty());
}

TEST(FactsResponse, AnEmptyResponseHasNoSets) {
  const response out;
  EXPECT_EQ(out.to_json(), R"({"sets":{}})");
}

TEST(FactsResponse, BuildsASetOfScalars) {
  response out;
  section os = out.set("os");
  os.value("family", "linux").value("version", "6.1.0").value("cores", 64).value("virtual", true);
  const boost::json::value document = document_of(out);
  const boost::json::object &os_json = sets_of(document).at("os").as_object();
  EXPECT_EQ(os_json.at("family").as_string(), "linux");
  EXPECT_EQ(os_json.at("cores").as_int64(), 64);
  EXPECT_TRUE(os_json.at("virtual").as_bool());
}

TEST(FactsResponse, AnEmptyStringIsNotAValue) {
  response out;
  out.set("os").value("family", "linux").value("build", "").value("kernel", static_cast<const char *>(nullptr));
  const boost::json::value document = document_of(out);
  const boost::json::object &os = sets_of(document).at("os").as_object();
  EXPECT_TRUE(os.if_contains("build") == nullptr) << "an unknown value is omitted, never written as an empty string";
  EXPECT_TRUE(os.if_contains("kernel") == nullptr);
  EXPECT_EQ(os.size(), 1u);
}

TEST(FactsResponse, HandlesStayValidWhileOtherSetsAreAdded) {
  response out;
  section os = out.set("os");
  out.set("hardware").value("vendor", "Dell Inc.");
  // The producer keeps filling the first set after starting the second; the
  // handle must still point at the first.
  os.value("family", "linux");
  const boost::json::value document = document_of(out);
  const boost::json::object &sets = sets_of(document);
  EXPECT_EQ(sets.at("os").as_object().at("family").as_string(), "linux");
  EXPECT_EQ(sets.at("hardware").as_object().at("vendor").as_string(), "Dell Inc.");
}

TEST(FactsResponse, TheSameSetTwiceIsTheSameSection) {
  response out;
  out.set("software").list("installed").record("nscp").value("version", "0.20.0");
  out.set("software").list("hotfixes").record("KB1");
  const boost::json::value document = document_of(out);
  const boost::json::object &software = sets_of(document).at("software").as_object();
  EXPECT_EQ(software.at("installed").as_array().size(), 1u);
  EXPECT_EQ(software.at("hotfixes").as_array().size(), 1u);
}

TEST(FactsResponse, RecordsCarryTheirId) {
  response out;
  record_list volumes = out.set("storage").list("volumes");
  volumes.record("c:").value("fs", "NTFS").value("size_bytes", 255000000000LL);
  volumes.record("d:").value("fs", "NTFS");
  const boost::json::value document = document_of(out);
  const boost::json::array &records = sets_of(document).at("storage").as_object().at("volumes").as_array();
  ASSERT_EQ(records.size(), 2u);
  EXPECT_EQ(records.at(0).as_object().at("id").as_string(), "c:");
  EXPECT_EQ(records.at(0).as_object().at("size_bytes").as_int64(), 255000000000LL);
  EXPECT_EQ(records.at(1).as_object().at("id").as_string(), "d:");
}

TEST(FactsResponse, ARecordWithoutAnIdIsReported) {
  response out;
  out.set("storage").list("volumes").record("").value("fs", "NTFS");
  const boost::json::value document = document_of(out);
  ASSERT_TRUE(document.as_object().if_contains("errors") != nullptr);
  EXPECT_NE(json_to_string(document.as_object().at("errors").as_object().at("storage").as_string()).find("without an id"), std::string::npos);
}

TEST(FactsResponse, NestedObjectsAndStringListsWork) {
  response out;
  section hardware = out.set("hardware");
  hardware.object("cpu").value("model", "Xeon").value("cores", 64);
  out.set("network").list("interfaces").record("eth0").strings("addresses", {"10.0.0.5", "", "10.0.0.6"});
  const boost::json::value document = document_of(out);
  const boost::json::object &sets = sets_of(document);
  EXPECT_EQ(sets.at("hardware").as_object().at("cpu").as_object().at("model").as_string(), "Xeon");
  const boost::json::array &addresses = sets.at("network").as_object().at("interfaces").as_array().at(0).as_object().at("addresses").as_array();
  ASSERT_EQ(addresses.size(), 2u) << "an empty string is skipped in a list too";
  EXPECT_EQ(addresses.at(0).as_string(), "10.0.0.5");
}

TEST(FactsResponse, TimestampsAreFormattedTheOneAllowedWay) {
  response out;
  // 2026-09-19T14:03:11Z
  out.set("os").time("boot_time", 1789826591).date("installed", 1789826591).time("never", 0);
  const boost::json::value document = document_of(out);
  const boost::json::object &os = sets_of(document).at("os").as_object();
  EXPECT_EQ(os.at("boot_time").as_string(), "2026-09-19T14:03:11Z");
  EXPECT_EQ(os.at("installed").as_string(), "2026-09-19");
  EXPECT_TRUE(os.if_contains("never") == nullptr) << "an unknown time is omitted, like an unknown string";
}

TEST(FactsResponse, AnInvalidKeyIsDroppedAndReported) {
  response out;
  out.set("os").value("family", "linux").value("Boot Time", "now");
  const boost::json::value document = document_of(out);
  const boost::json::object &os = sets_of(document).at("os").as_object();
  EXPECT_EQ(os.size(), 1u) << "the rest of the set still ships";
  ASSERT_TRUE(document.as_object().if_contains("errors") != nullptr);
  EXPECT_NE(json_to_string(document.as_object().at("errors").as_object().at("os").as_string()).find("Boot Time"), std::string::npos);
}

TEST(FactsResponse, RemoveIsAnExplicitNull) {
  response out;
  out.set("docker").value("version", "26.1.0");
  out.remove("docker");
  const boost::json::value document = document_of(out);
  const boost::json::object &sets = sets_of(document);
  ASSERT_TRUE(sets.if_contains("docker") != nullptr);
  EXPECT_TRUE(sets.at("docker").is_null()) << "not returning a set keeps the old value; a null drops it";
}

TEST(FactsResponse, ErrorsAreReportedPerSet) {
  response out;
  out.error("software.installed", "access denied to HKLM");
  const boost::json::value document = document_of(out);
  EXPECT_EQ(document.as_object().at("errors").as_object().at("software.installed").as_string(), "access denied to HKLM");
}

TEST(FactsResponse, ErrorAllOnlyTouchesWhatThisRoundWanted) {
  const request req(request_for("\"os\""));
  response out;
  out.error_all(req, {"os", "hardware"}, "the collector threw");
  const boost::json::value document = document_of(out);
  const boost::json::object &errors = document.as_object().at("errors").as_object();
  EXPECT_EQ(errors.size(), 1u);
  EXPECT_TRUE(errors.if_contains("os") != nullptr);
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
  EXPECT_EQ(parse_document(R"({"revision":3,"facts":{"os":{"family":"linux"}}})").as_object().at("os").as_object().at("family").as_string(), "linux");
  // A core without the facts API answers "{}", and a subtree that is not
  // there has no facts member at all.
  EXPECT_TRUE(parse_document("{}").as_object().empty());
  EXPECT_TRUE(parse_document(R"({"found":false})").as_object().empty());
  EXPECT_TRUE(parse_document("not json").as_object().empty());
}
