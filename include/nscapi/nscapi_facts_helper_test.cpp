// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_helper.hpp>
#include <string>
#include <vector>

using namespace nscapi::facts;

namespace {
// The `sets` half of the response, which is what every assertion below is
// about; the errors half is asserted separately.
std::string sets_of(const response &out) {
  const std::string body = out.serialize();
  const std::string prefix = "{\"sets\":";
  const std::string suffix = ",\"errors\":{}}";
  if (body.compare(0, prefix.size(), prefix) != 0) return body;
  if (body.size() < prefix.size() + suffix.size()) return body;
  if (body.compare(body.size() - suffix.size(), suffix.size(), suffix) != 0) return body;
  return body.substr(prefix.size(), body.size() - prefix.size() - suffix.size());
}
}  // namespace

// ============================================================================
// request
// ============================================================================

TEST(FactsHelperRequest, ParsesTheEnabledListAndTheReason) {
  const request req = request::parse(R"({"enabled":["os","software.installed"],"reason":"scheduled"})");
  EXPECT_TRUE(req.wants("os"));
  EXPECT_TRUE(req.wants("software.installed"));
  EXPECT_FALSE(req.wants("hardware"));
  EXPECT_EQ("scheduled", req.reason());
  EXPECT_FALSE(req.is_manual());
}

TEST(FactsHelperRequest, RecognisesAManualRound) {
  EXPECT_TRUE(request::parse(R"({"enabled":[],"reason":"manual"})").is_manual());
  EXPECT_FALSE(request::parse(R"({"enabled":[],"reason":"startup"})").is_manual());
}

TEST(FactsHelperRequest, AnEmptyEnabledListWantsNothing) {
  const request req = request::parse(R"({"enabled":[],"reason":"startup"})");
  EXPECT_TRUE(req.enabled().empty());
  EXPECT_FALSE(req.wants("os"));
}

// A producer that cannot tell what was enabled must publish nothing, not
// everything: the enable list is the whole privacy contract.
TEST(FactsHelperRequest, AMalformedRequestWantsNothing) {
  EXPECT_FALSE(request::parse("").wants("os"));
  EXPECT_FALSE(request::parse("not json").wants("os"));
  EXPECT_FALSE(request::parse(R"({"enabled":)").wants("os"));
  EXPECT_FALSE(request::parse("{}").wants("os"));
}

TEST(FactsHelperRequest, KeysItDoesNotKnowAreIgnoredRatherThanAbortingTheParse) {
  const request req = request::parse(R"({"future":"whatever","enabled":["os"],"reason":"reload"})");
  EXPECT_TRUE(req.wants("os"));
  EXPECT_EQ("reload", req.reason());
}

TEST(FactsHelperRequest, EscapesInTheRequestAreResolved) {
  const request req = request::parse(R"({"enabled":["os"],"reason":"man\u0075al"})");
  EXPECT_TRUE(req.is_manual());
}

// ============================================================================
// Scalars
// ============================================================================

TEST(FactsHelperSection, WritesStringsNumbersAndBooleans) {
  response out;
  section os = out.set("os");
  os.value("family", "linux").value("version", "6.8.0");
  os.value("build", 20348).value("pending_reboot", false);
  EXPECT_EQ(R"({"os":{"family":"linux","version":"6.8.0","build":20348,"pending_reboot":false}})", sets_of(out));
}

TEST(FactsHelperSection, AnEmptyStringIsOmittedRatherThanWrittenAsEmpty) {
  response out;
  out.set("os").value("family", "linux").value("name", std::string()).value("version", static_cast<const char *>(nullptr));
  EXPECT_EQ(R"({"os":{"family":"linux"}})", sets_of(out));
}

TEST(FactsHelperSection, ZeroAndFalseAreRealValuesAndAreWritten) {
  response out;
  out.set("hardware").value("sockets", 0).value("virtual", false);
  EXPECT_EQ(R"({"hardware":{"sockets":0,"virtual":false}})", sets_of(out));
}

TEST(FactsHelperSection, LargeUnsignedValuesSurviveWithoutOverflowing) {
  response out;
  out.set("hardware").value("total_bytes", 18446744073709551615ull);
  EXPECT_EQ(R"({"hardware":{"total_bytes":18446744073709551615}})", sets_of(out));
}

TEST(FactsHelperSection, DoublesAreWrittenInTheirShortestRoundTripForm) {
  response out;
  out.set("hardware").value("load", 0.1).value("ratio", 1.5);
  EXPECT_EQ(R"({"hardware":{"load":0.1,"ratio":1.5}})", sets_of(out));
}

TEST(FactsHelperSection, WritingTheSameKeyTwiceKeepsTheLastValueNotBoth) {
  response out;
  out.set("os").value("family", "linux").value("family", "windows");
  EXPECT_EQ(R"({"os":{"family":"windows"}})", sets_of(out));
}

TEST(FactsHelperSection, StringsAreEscapedSoTheCoreCanParseWhatWasWritten) {
  response out;
  out.set("os").value("name", "a \"quoted\" \\ name\nwith a newline\tand a tab");
  EXPECT_EQ(R"({"os":{"name":"a \"quoted\" \\ name\nwith a newline\tand a tab"}})", sets_of(out));
}

TEST(FactsHelperSection, ControlCharactersAreEscapedAsUnicodeCodePoints) {
  response out;
  out.set("os").value("name", std::string("bell\x07"));
  EXPECT_EQ(R"({"os":{"name":"bell\u0007"}})", sets_of(out));
}

// ============================================================================
// Timestamps
// ============================================================================

TEST(FactsHelperSection, TimeRendersIso8601Utc) {
  response out;
  out.set("os").time("boot_time", static_cast<std::time_t>(1788000000));
  EXPECT_EQ(R"({"os":{"boot_time":"2026-08-29T10:40:00Z"}})", sets_of(out));
}

TEST(FactsHelperSection, DateRendersACalendarDay) {
  response out;
  out.set("software").date("checked", static_cast<std::time_t>(1788000000));
  EXPECT_EQ(R"({"software":{"checked":"2026-08-29"}})", sets_of(out));
}

// A package manager that never recorded an install date hands a producer a
// zero; writing "1970-01-01" would be a fact the host does not have.
TEST(FactsHelperSection, AnUnsetTimestampIsOmitted) {
  response out;
  out.set("os").value("family", "linux").time("boot_time", static_cast<std::time_t>(0)).date("installed", static_cast<std::time_t>(0));
  EXPECT_EQ(R"({"os":{"family":"linux"}})", sets_of(out));
}

TEST(FactsHelperSection, AnAlreadyFormattedTimestampIsPassedThrough) {
  response out;
  out.set("os").time("boot_time", std::string("2026-09-01T04:12:09Z"));
  EXPECT_EQ(R"({"os":{"boot_time":"2026-09-01T04:12:09Z"}})", sets_of(out));
}

// ============================================================================
// Nesting, lists and records
// ============================================================================

TEST(FactsHelperSection, SubNestsAnObject) {
  response out;
  section hardware = out.set("hardware");
  hardware.value("vendor", "Dell Inc.");
  hardware.sub("cpu").value("model", "Intel Xeon Gold 6338").value("sockets", 2);
  EXPECT_EQ(R"({"hardware":{"vendor":"Dell Inc.","cpu":{"model":"Intel Xeon Gold 6338","sockets":2}}})", sets_of(out));
}

TEST(FactsHelperSection, SubReturnsTheSameObjectWhenCalledTwice) {
  response out;
  section hardware = out.set("hardware");
  hardware.sub("cpu").value("sockets", 2);
  hardware.sub("cpu").value("cores", 64);
  EXPECT_EQ(R"({"hardware":{"cpu":{"sockets":2,"cores":64}}})", sets_of(out));
}

TEST(FactsHelperList, RecordWritesTheIdItself) {
  response out;
  list volumes = out.set("storage").list("volumes");
  volumes.record("/").value("fs", "ext4").value("size_bytes", 255000000000ll);
  volumes.record("/boot").value("fs", "vfat");
  EXPECT_EQ(R"({"storage":{"volumes":[{"id":"/","fs":"ext4","size_bytes":255000000000},{"id":"/boot","fs":"vfat"}]}})", sets_of(out));
  EXPECT_EQ(2u, volumes.size());
}

// The core rejects the set with "has a record without a non-empty string
// 'id'", which names the producer's bug. Inventing an id here would hide it.
TEST(FactsHelperList, ARecordWithAnEmptyIdIsAppendedWithoutOneSoTheCoreRejectsTheSet) {
  response out;
  out.set("storage").list("volumes").record("").value("fs", "ext4");
  EXPECT_EQ(R"({"storage":{"volumes":[{"fs":"ext4"}]}})", sets_of(out));
}

TEST(FactsHelperList, AnEmptyListIsWrittenBecauseNoneIsNotTheSameAsNotCollected) {
  response out;
  out.set("storage").list("volumes");
  EXPECT_EQ(R"({"storage":{"volumes":[]}})", sets_of(out));
}

TEST(FactsHelperList, TheSameListNameAppendsRatherThanStartingOver) {
  response out;
  out.set("storage").list("volumes").record("/");
  out.set("storage").list("volumes").record("/boot");
  EXPECT_EQ(R"({"storage":{"volumes":[{"id":"/"},{"id":"/boot"}]}})", sets_of(out));
}

TEST(FactsHelperSection, StringsWritesAPlainListAndDropsEmptyEntries) {
  response out;
  std::vector<std::string> addresses;
  addresses.push_back("10.0.0.5");
  addresses.push_back("");
  addresses.push_back("fe80::1");
  out.set("agent").strings("addresses", addresses);
  EXPECT_EQ(R"({"agent":{"addresses":["10.0.0.5","fe80::1"]}})", sets_of(out));
}

TEST(FactsHelperSection, StringsWithNothingInItStillWritesAnEmptyList) {
  response out;
  out.set("agent").strings("modules", std::vector<std::string>());
  EXPECT_EQ(R"({"agent":{"modules":[]}})", sets_of(out));
}

// ============================================================================
// The response as a whole
// ============================================================================

TEST(FactsHelperResponse, AFreshResponseProducesNothing) {
  const response out;
  EXPECT_EQ(R"({"sets":{},"errors":{}})", out.serialize());
}

TEST(FactsHelperResponse, TwoDottedSetsUnderOneKeyAreProducedIndependently) {
  response out;
  out.set("software").list("installed").record("curl").value("version", "8.5.0");
  out.set("software").list("hotfixes").record("KB5000001");
  EXPECT_EQ(R"({"software":{"installed":[{"id":"curl","version":"8.5.0"}],"hotfixes":[{"id":"KB5000001"}]}})", sets_of(out));
}

TEST(FactsHelperResponse, ErrorsAreReportedPerSetAndDoNotProduceASet) {
  response out;
  out.set("os").value("family", "linux");
  out.error("software.installed", "access denied to HKLM\\SOFTWARE");
  EXPECT_EQ(R"({"sets":{"os":{"family":"linux"}},"errors":{"software.installed":"access denied to HKLM\\SOFTWARE"}})", out.serialize());
}

TEST(FactsHelperResponse, ReportingTwoErrorsForOneSetKeepsTheLast) {
  response out;
  out.error("docker", "socket missing");
  out.error("docker", "connection refused");
  EXPECT_EQ(R"({"sets":{},"errors":{"docker":"connection refused"}})", out.serialize());
}

// "Gone for good" is a different statement from "failed this round": one
// removes the set, the other leaves the previous value alone.
TEST(FactsHelperResponse, RemoveWritesAnExplicitNullForATopLevelSet) {
  response out;
  out.remove("docker");
  EXPECT_EQ(R"({"docker":null})", sets_of(out));
}

TEST(FactsHelperResponse, RemoveWritesAnExplicitNullAtTheDottedPath) {
  response out;
  out.set("software").list("hotfixes").record("KB5000001");
  out.remove("software.installed");
  EXPECT_EQ(R"({"software":{"hotfixes":[{"id":"KB5000001"}],"installed":null}})", sets_of(out));
}

// ============================================================================
// The formatting helpers, used directly by producers that build an id
// ============================================================================

TEST(FactsHelperFormatting, QuoteProducesAJsonStringLiteral) {
  EXPECT_EQ(R"("")", quote(""));
  EXPECT_EQ(R"("c:\\")", quote("c:\\"));
  EXPECT_EQ(R"("Ethernet 0")", quote("Ethernet 0"));
}

TEST(FactsHelperFormatting, TheTimestampHelpersAgreeWithTheSectionMethods) {
  EXPECT_EQ("2026-08-29T10:40:00Z", to_iso8601(static_cast<std::time_t>(1788000000)));
  EXPECT_EQ("2026-08-29", to_date(static_cast<std::time_t>(1788000000)));
  EXPECT_EQ("", to_iso8601(static_cast<std::time_t>(0)));
}
