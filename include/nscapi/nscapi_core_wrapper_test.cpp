// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_core_wrapper.hpp>

// core_wrapper::parse_tags_json turns the flat {"k":"v",...} object the core
// hands back (get_tags_json) into a typed map, so a consuming module never has
// to link a JSON parser. These cover the shapes the core actually emits plus
// the malformed inputs the parser has to survive without throwing.

using nscapi::core_wrapper;

TEST(ParseTagsJson, EmptyObjectIsEmpty) { EXPECT_TRUE(core_wrapper::parse_tags_json("{}").empty()); }

TEST(ParseTagsJson, MissingApiSentinelIsEmpty) {
  // get_tags_json() returns "{}" on a core without the tag API.
  EXPECT_TRUE(core_wrapper::parse_tags_json("{}").empty());
  EXPECT_TRUE(core_wrapper::parse_tags_json("").empty());
}

TEST(ParseTagsJson, SimplePairs) {
  const auto tags = core_wrapper::parse_tags_json(R"({"drives":"c:,d:","sqlserver":"detected"})");
  ASSERT_EQ(tags.size(), 2u);
  EXPECT_EQ(tags.at("drives"), "c:,d:");
  EXPECT_EQ(tags.at("sqlserver"), "detected");
}

TEST(ParseTagsJson, ToleratesInsignificantWhitespace) {
  const auto tags = core_wrapper::parse_tags_json("{ \"a\" : \"1\" , \"b\":\"2\" }");
  ASSERT_EQ(tags.size(), 2u);
  EXPECT_EQ(tags.at("a"), "1");
  EXPECT_EQ(tags.at("b"), "2");
}

TEST(ParseTagsJson, ResolvesStringEscapes) {
  // Quote, backslash, slash and the short control escapes round-trip.
  const auto tags = core_wrapper::parse_tags_json(R"({"k":"a\"b\\c\/d\ne\tf"})");
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags.at("k"), "a\"b\\c/d\ne\tf");
}

TEST(ParseTagsJson, ResolvesUnicodeEscapedControlChar) {
  // The core \u-escapes control characters; 	 is a tab.
  const auto tags = core_wrapper::parse_tags_json(R"({"k":"a	b"})");
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags.at("k"), "a\tb");
}

TEST(ParseTagsJson, KeepsRawUtf8InValues) {
  // Non-ASCII is emitted raw (not \u-escaped), so it must pass through verbatim.
  const std::string utf8 = "caf\xC3\xA9";  // café
  const auto tags = core_wrapper::parse_tags_json("{\"k\":\"" + utf8 + "\"}");
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags.at("k"), utf8);
}

TEST(ParseTagsJson, EmptyValueIsKept) {
  const auto tags = core_wrapper::parse_tags_json(R"({"k":""})");
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags.at("k"), "");
}

TEST(ParseTagsJson, EscapesInKeys) {
  const auto tags = core_wrapper::parse_tags_json(R"({"a\"b":"v"})");
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags.at("a\"b"), "v");
}

TEST(ParseTagsJson, NonObjectReturnsEmpty) {
  EXPECT_TRUE(core_wrapper::parse_tags_json("[]").empty());
  EXPECT_TRUE(core_wrapper::parse_tags_json("\"just a string\"").empty());
  EXPECT_TRUE(core_wrapper::parse_tags_json("garbage").empty());
}

TEST(ParseTagsJson, MalformedInputYieldsWhatWasParsedNotAThrow) {
  // A truncated object returns the pairs read before the break rather than
  // throwing - a module reading tags must never crash on a bad buffer.
  const auto tags = core_wrapper::parse_tags_json(R"({"a":"1","b":)");
  EXPECT_EQ(tags.at("a"), "1");
  EXPECT_EQ(tags.count("b"), 0u);
}

TEST(ParseTagsJson, UnterminatedStringIsNotAThrow) {
  EXPECT_NO_THROW(core_wrapper::parse_tags_json(R"({"a":"unterminated)"));
}
