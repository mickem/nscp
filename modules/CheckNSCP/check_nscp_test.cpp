// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nsclient/nsclient_exception.hpp>
#include <string>

#include "check_nscp_helpers.hpp"

using check_nscp_helpers::compare;
using check_nscp_helpers::nscp_version;
using check_nscp_helpers::parse_releases_payload;
using check_nscp_helpers::sanitize_tag;

TEST(CheckNSCPVersion, ParseThreeComponent) {
  const nscp_version v("0.6.5");
  EXPECT_EQ(v.release, 0);
  EXPECT_EQ(v.major_version, 6);
  EXPECT_EQ(v.minor_version, 5);
  EXPECT_FALSE(v.has_build);
  EXPECT_EQ(v.to_string(), "0.6.5");
}

TEST(CheckNSCPVersion, ParseFourComponent) {
  const nscp_version v("0.5.2.35");
  EXPECT_EQ(v.release, 0);
  EXPECT_EQ(v.major_version, 5);
  EXPECT_EQ(v.minor_version, 2);
  EXPECT_EQ(v.build, 35);
  EXPECT_TRUE(v.has_build);
  EXPECT_EQ(v.to_string(), "0.5.2.35");
}

TEST(CheckNSCPVersion, ParseExtractsDateSuffix) {
  // getApplicationVersionString() returns "<version> <date>"; the date part
  // ends up in the date field and is preserved verbatim.
  const nscp_version v("0.6.5 2025-01-02");
  EXPECT_EQ(v.to_string(), "0.6.5");
  EXPECT_EQ(v.date, "2025-01-02");
}

TEST(CheckNSCPVersion, ParseEmptyThrows) { EXPECT_THROW(nscp_version{""}, nsclient::nsclient_exception); }

TEST(CheckNSCPVersion, ParseTooManyComponentsThrows) { EXPECT_THROW(nscp_version("1.2.3.4.5"), nsclient::nsclient_exception); }

TEST(SanitizeTag, StripsLeadingV) {
  EXPECT_EQ(sanitize_tag("v0.6.5"), "0.6.5");
  EXPECT_EQ(sanitize_tag("V1.2.3"), "1.2.3");
}

TEST(SanitizeTag, TruncatesAtNonVersionCharacter) {
  EXPECT_EQ(sanitize_tag("0.6.5-rc1"), "0.6.5");
  EXPECT_EQ(sanitize_tag("v0.6.5+build.7"), "0.6.5");
}

TEST(SanitizeTag, PassesThroughCleanTag) {
  EXPECT_EQ(sanitize_tag("0.6.5"), "0.6.5");
  EXPECT_EQ(sanitize_tag("0.5.2.35"), "0.5.2.35");
}

TEST(SanitizeTag, EmptyStaysEmpty) { EXPECT_EQ(sanitize_tag(""), ""); }

TEST(VersionCompare, Equal) {
  const nscp_version a("0.6.5");
  const nscp_version b("0.6.5");
  EXPECT_EQ(compare(a, b), 0);
}

TEST(VersionCompare, ReleaseDominates) {
  const nscp_version a("0.9.9");
  const nscp_version b("1.0.0");
  EXPECT_LT(compare(a, b), 0);
  EXPECT_GT(compare(b, a), 0);
}

TEST(VersionCompare, MajorThenMinor) {
  EXPECT_LT(compare(nscp_version("0.6.5"), nscp_version("0.7.0")), 0);
  EXPECT_LT(compare(nscp_version("0.6.4"), nscp_version("0.6.5")), 0);
  EXPECT_GT(compare(nscp_version("0.6.5"), nscp_version("0.6.4")), 0);
}

TEST(VersionCompare, BuildOnlyConsideredWhenBothPresent) {
  // 0.6.5 vs 0.6.5.10 -> equal (one side has no build component).
  EXPECT_EQ(compare(nscp_version("0.6.5"), nscp_version("0.6.5.10")), 0);
  // 0.6.5.5 vs 0.6.5.10 -> latter newer.
  EXPECT_LT(compare(nscp_version("0.6.5.5"), nscp_version("0.6.5.10")), 0);
}

namespace {
const char *kStableArrayPayload = R"([
  {"tag_name":"v0.6.6","html_url":"https://example/v0.6.6","published_at":"2026-04-01T00:00:00Z","draft":false,"prerelease":false},
  {"tag_name":"v0.6.5","html_url":"https://example/v0.6.5","published_at":"2026-01-01T00:00:00Z","draft":false,"prerelease":false}
])";

const char *kPrereleaseFirstPayload = R"([
  {"tag_name":"v0.7.0-rc1","html_url":"https://example/rc1","published_at":"2026-04-15T00:00:00Z","draft":false,"prerelease":true},
  {"tag_name":"v0.6.6","html_url":"https://example/v0.6.6","published_at":"2026-04-01T00:00:00Z","draft":false,"prerelease":false}
])";

const char *kDraftThenStablePayload = R"([
  {"tag_name":"v0.7.0","html_url":"https://example/draft","published_at":"2026-04-20T00:00:00Z","draft":true,"prerelease":false},
  {"tag_name":"v0.6.6","html_url":"https://example/v0.6.6","published_at":"2026-04-01T00:00:00Z","draft":false,"prerelease":false}
])";

const char *kSingleObjectPayload =
    R"({"tag_name":"v0.6.6","html_url":"https://example/v0.6.6","published_at":"2026-04-01T00:00:00Z","draft":false,"prerelease":false})";
}  // namespace

TEST(ParseReleasesPayload, PicksFirstStableFromArray) {
  std::string tag, url, published, error;
  ASSERT_TRUE(parse_releases_payload(kStableArrayPayload, /*include_prerelease=*/false, tag, url, published, error));
  EXPECT_EQ(tag, "v0.6.6");
  EXPECT_EQ(url, "https://example/v0.6.6");
  EXPECT_EQ(published, "2026-04-01T00:00:00Z");
  EXPECT_TRUE(error.empty());
}

TEST(ParseReleasesPayload, SkipsPrereleaseWhenStableOnly) {
  std::string tag, url, published, error;
  ASSERT_TRUE(parse_releases_payload(kPrereleaseFirstPayload, /*include_prerelease=*/false, tag, url, published, error));
  EXPECT_EQ(tag, "v0.6.6");
}

TEST(ParseReleasesPayload, IncludesPrereleaseWhenAsked) {
  std::string tag, url, published, error;
  ASSERT_TRUE(parse_releases_payload(kPrereleaseFirstPayload, /*include_prerelease=*/true, tag, url, published, error));
  EXPECT_EQ(tag, "v0.7.0-rc1");
}

TEST(ParseReleasesPayload, SkipsDrafts) {
  std::string tag, url, published, error;
  // Even with prereleases enabled, a draft must be skipped.
  ASSERT_TRUE(parse_releases_payload(kDraftThenStablePayload, /*include_prerelease=*/true, tag, url, published, error));
  EXPECT_EQ(tag, "v0.6.6");
}

TEST(ParseReleasesPayload, AcceptsSingleObject) {
  // /releases/latest returns one object, not an array.
  std::string tag, url, published, error;
  ASSERT_TRUE(parse_releases_payload(kSingleObjectPayload, /*include_prerelease=*/false, tag, url, published, error));
  EXPECT_EQ(tag, "v0.6.6");
  EXPECT_EQ(url, "https://example/v0.6.6");
}

TEST(ParseReleasesPayload, NoStableReleasesReportsError) {
  const char *payload = R"([
    {"tag_name":"v0.7.0-rc1","draft":false,"prerelease":true},
    {"tag_name":"v0.7.0-draft","draft":true,"prerelease":false}
  ])";
  std::string tag, url, published, error;
  EXPECT_FALSE(parse_releases_payload(payload, /*include_prerelease=*/false, tag, url, published, error));
  EXPECT_EQ(error, "no stable releases found");
}

TEST(ParseReleasesPayload, EmptyArrayReportsError) {
  std::string tag, url, published, error;
  EXPECT_FALSE(parse_releases_payload("[]", /*include_prerelease=*/true, tag, url, published, error));
  EXPECT_EQ(error, "no releases found");
}

TEST(ParseReleasesPayload, FilteredSingleObjectReportsError) {
  const char *payload = R"({"tag_name":"v0.7.0","draft":true})";
  std::string tag, url, published, error;
  EXPECT_FALSE(parse_releases_payload(payload, /*include_prerelease=*/true, tag, url, published, error));
  EXPECT_EQ(error, "release was filtered out (draft or pre-release)");
}

TEST(ParseReleasesPayload, MalformedJsonReportsError) {
  std::string tag, url, published, error;
  EXPECT_FALSE(parse_releases_payload("not-json", /*include_prerelease=*/false, tag, url, published, error));
  EXPECT_FALSE(error.empty());
  EXPECT_NE(error.find("failed to parse JSON"), std::string::npos);
}

TEST(ParseReleasesPayload, UnexpectedShapeReportsError) {
  std::string tag, url, published, error;
  EXPECT_FALSE(parse_releases_payload("42", /*include_prerelease=*/false, tag, url, published, error));
  EXPECT_EQ(error, "unexpected JSON shape in response");
}

TEST(ParseReleasesPayload, MissingTagSkipsEntry) {
  // First entry has no tag_name, so the parser should skip it and pick the
  // next one with a tag.
  const char *payload = R"([
    {"html_url":"https://example/no-tag","draft":false,"prerelease":false},
    {"tag_name":"v0.6.6","draft":false,"prerelease":false}
  ])";
  std::string tag, url, published, error;
  ASSERT_TRUE(parse_releases_payload(payload, /*include_prerelease=*/false, tag, url, published, error));
  EXPECT_EQ(tag, "v0.6.6");
}

// ---------------------------------------------------------------------------
// check_nscp (agent health)
// ---------------------------------------------------------------------------

using check_nscp_helpers::crash_scan;
using check_nscp_helpers::extension_of;
using check_nscp_helpers::health_obj;
using check_nscp_helpers::is_crash_report;

TEST(ExtensionOf, ReturnsExtensionWithLeadingDot) {
  EXPECT_EQ(extension_of("2025-01-02-12-00-00.crash"), ".crash");
  EXPECT_EQ(extension_of("dump.dmp"), ".dmp");
}

TEST(ExtensionOf, LowerCasesTheExtension) {
  EXPECT_EQ(extension_of("REPORT.CRASH"), ".crash");
  EXPECT_EQ(extension_of("Report.TxT"), ".txt");
}

TEST(ExtensionOf, IgnoresDirectoriesInThePath) {
  EXPECT_EQ(extension_of("/var/lib/nsclient.d/crash-dumps/report.crash"), ".crash");
  // A dot in a directory name must not be mistaken for the file's extension.
  EXPECT_EQ(extension_of("/var/lib/nsclient.d/crash-dumps/report"), "");
  EXPECT_EQ(extension_of("C:\\Program Files\\NSClient++\\crash dumps\\report.crash"), ".crash");
}

TEST(ExtensionOf, NoExtensionIsEmpty) {
  EXPECT_EQ(extension_of("report"), "");
  EXPECT_EQ(extension_of(""), "");
  // A leading dot is part of the name, not an extension.
  EXPECT_EQ(extension_of(".nscp"), "");
}

TEST(IsCrashReport, AcceptsTheArchivedReportExtensions) {
  // What the crash handler writes today, plus minidumps and the plain text
  // reports older releases produced.
  EXPECT_TRUE(is_crash_report("2025-01-02-12-00-00.crash"));
  EXPECT_TRUE(is_crash_report("crash.dmp"));
  EXPECT_TRUE(is_crash_report("crash.txt"));
  EXPECT_TRUE(is_crash_report("CRASH.CRASH"));
}

TEST(IsCrashReport, RejectsEverythingElse) {
  EXPECT_FALSE(is_crash_report("nsclient.log"));
  EXPECT_FALSE(is_crash_report("readme"));
  EXPECT_FALSE(is_crash_report("report.crash.bak"));
  EXPECT_FALSE(is_crash_report(""));
}

TEST(CrashScan, EmptyScanHasNoCrashes) {
  const crash_scan scan;
  EXPECT_EQ(scan.count, 0);
  EXPECT_EQ(scan.newest, "");
  EXPECT_FALSE(scan.has_newest);
  EXPECT_FALSE(scan.age(1000));
}

TEST(CrashScan, CountsOnlyCrashReports) {
  crash_scan scan;
  scan.add("a.crash", 100);
  scan.add("nsclient.log", 200);
  scan.add("b.dmp", 150);
  scan.add("cache", 300);
  EXPECT_EQ(scan.count, 2);
}

TEST(CrashScan, NewestReportWins) {
  crash_scan scan;
  scan.add("old.crash", 100);
  scan.add("new.crash", 300);
  scan.add("middle.crash", 200);
  EXPECT_EQ(scan.newest, "new.crash");
  EXPECT_EQ(scan.newest_time, 300);
  EXPECT_EQ(scan.count, 3);
}

TEST(CrashScan, NonReportsNeverBecomeTheNewest) {
  // The historical bug: the newest-file bookkeeping ran over every directory
  // entry while only ".txt" files were counted, so ${last_crash} could name a
  // file that is not a crash report at all.
  crash_scan scan;
  scan.add("old.crash", 100);
  scan.add("nsclient.log", 5000);
  EXPECT_EQ(scan.newest, "old.crash");
  EXPECT_EQ(scan.count, 1);
}

TEST(CrashScan, AnUndatableReportStillCounts) {
  // The scan could not read the file's modification time. The report is there,
  // so it counts - but it must not be dated to the epoch.
  crash_scan scan;
  scan.add("undatable.crash", boost::none);
  EXPECT_EQ(scan.count, 1);
  EXPECT_FALSE(scan.has_newest);
  EXPECT_EQ(scan.newest, "");
  EXPECT_FALSE(scan.age(1000));
}

TEST(CrashScan, AnUndatableReportNeverBecomesTheNewest) {
  crash_scan scan;
  scan.add("dated.crash", 500);
  scan.add("undatable.crash", boost::none);
  EXPECT_EQ(scan.count, 2);
  EXPECT_EQ(scan.newest, "dated.crash");
  const boost::optional<long long> age = scan.age(800);
  ASSERT_TRUE(age);
  EXPECT_EQ(*age, 300);
}

TEST(CrashScan, AnUndatableReportDoesNotShadowALaterOne) {
  // Order must not matter: the undatable entry is simply not a candidate.
  crash_scan scan;
  scan.add("undatable.crash", boost::none);
  scan.add("dated.crash", 500);
  EXPECT_EQ(scan.newest, "dated.crash");
  EXPECT_EQ(scan.newest_time, 500);
}

TEST(CrashScan, AgeIsRelativeToNow) {
  crash_scan scan;
  scan.add("a.crash", 1000);
  const boost::optional<long long> age = scan.age(1600);
  ASSERT_TRUE(age);
  EXPECT_EQ(*age, 600);
}

TEST(CrashScan, AgeOfAFutureReportIsClampedToZero) {
  // Clock skew or a restored backup must not produce a negative age.
  crash_scan scan;
  scan.add("a.crash", 2000);
  const boost::optional<long long> age = scan.age(1000);
  ASSERT_TRUE(age);
  EXPECT_EQ(*age, 0);
}

TEST(HealthObj, DefaultsAreAHealthyAgent) {
  const health_obj obj;
  EXPECT_EQ(obj.get_crashes(), 0);
  EXPECT_EQ(obj.get_errors(), 0);
  EXPECT_EQ(obj.get_uptime(), 0);
  EXPECT_EQ(obj.get_last_crash(), "");
  EXPECT_EQ(obj.get_last_error(), "");
  EXPECT_FALSE(obj.get_crash_age());
  EXPECT_EQ(obj.get_crash_age_s(), "none");
}

TEST(HealthObj, RendersTheSummary) {
  health_obj obj;
  obj.crashes = 2;
  obj.errors = 3;
  obj.uptime = 3 * 60 * 60 + 25 * 60;
  EXPECT_EQ(obj.get_summary(), "2 crash(es), 3 error(s), uptime 03:25");
  // show() is what the filter engine falls back to for ${list} entries.
  EXPECT_EQ(obj.show(), obj.get_summary());
}

TEST(HealthObj, UptimeHonoursMaxUnit) {
  health_obj obj;
  obj.uptime = 6 * 7 * 24 * 60 * 60;  // six weeks
  obj.max_unit = str::format::unit_week;
  EXPECT_EQ(obj.get_uptime_s(), "6w 0d 00:00");
  obj.max_unit = str::format::unit_day;
  EXPECT_EQ(obj.get_uptime_s(), "42d 00:00");
  obj.max_unit = str::format::unit_hour;
  EXPECT_EQ(obj.get_uptime_s(), "1008:00");
}

TEST(HealthObj, CrashAgeRendersWithTheSameGranularity) {
  health_obj obj;
  obj.crash_age = 2 * 24 * 60 * 60;
  obj.max_unit = str::format::unit_day;
  EXPECT_EQ(obj.get_crash_age_s(), "2d 00:00");
  ASSERT_TRUE(obj.get_crash_age());
  EXPECT_EQ(*obj.get_crash_age(), 2 * 24 * 60 * 60);
}

TEST(HealthObj, NegativeDurationsRenderAsZero) {
  health_obj obj;
  obj.uptime = -1;
  EXPECT_EQ(obj.get_uptime_s(), "0");
}

TEST(HealthObj, CarriesTheLastCrashAndError) {
  health_obj obj;
  obj.last_crash = "2025-01-02-12-00-00.crash";
  obj.last_error = "Failed to load module";
  obj.version = "0.17.2";
  obj.date = "2026-01-02";
  EXPECT_EQ(obj.get_last_crash(), "2025-01-02-12-00-00.crash");
  EXPECT_EQ(obj.get_last_error(), "Failed to load module");
  EXPECT_EQ(obj.get_version(), "0.17.2");
  EXPECT_EQ(obj.get_date(), "2026-01-02");
}
