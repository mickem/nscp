/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
