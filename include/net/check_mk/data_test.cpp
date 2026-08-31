// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <net/check_mk/data.hpp>
#include <string>
#include <vector>

using check_mk::packet;

// =============================================================================
// line — tokenization and rendering
// =============================================================================

TEST(CheckMkLine, SetLineSplitsOnSpaces) {
  packet::section::line l("a b c");
  ASSERT_EQ(l.items.size(), 3u);
  EXPECT_EQ(l.get_item(0), "a");
  EXPECT_EQ(l.get_item(1), "b");
  EXPECT_EQ(l.get_item(2), "c");
}

TEST(CheckMkLine, GetItemOutOfRangeThrows) {
  packet::section::line l("a b");
  EXPECT_THROW(l.get_item(2), check_mk::check_mk_exception);
}

TEST(CheckMkLine, GetLineJoinsWithSpace) {
  packet::section::line l("one two three");
  EXPECT_EQ(l.get_line(), "one two three");
}

TEST(CheckMkLine, ToStringWithCustomSeparator) {
  packet::section::line l("a b c");
  EXPECT_EQ(l.to_string('|'), "a|b|c");
}

TEST(CheckMkLine, EmptyLineRendersEmpty) {
  packet::section::line l;
  EXPECT_EQ(l.to_string(), "");
}

TEST(CheckMkLine, SetRawKeepsSingleItem) {
  packet::section::line l("a b c");
  l.set_raw("x y z");
  ASSERT_EQ(l.items.size(), 1u);
  EXPECT_EQ(l.get_item(0), "x y z");
  EXPECT_EQ(l.to_string('|'), "x y z");
}

TEST(CheckMkLine, CopyAndAssign) {
  packet::section::line a("a b");
  packet::section::line b(a);
  packet::section::line c;
  c = a;
  EXPECT_EQ(b.get_line(), "a b");
  EXPECT_EQ(c.get_line(), "a b");
}

// =============================================================================
// section — header rendering with decorations
// =============================================================================

TEST(CheckMkSection, RenderHeaderPlain) {
  packet::section s("check_mk");
  EXPECT_EQ(s.render_header(), "<<<check_mk>>>\n");
}

TEST(CheckMkSection, RenderHeaderWithSeparator) {
  packet::section s("df");
  s.separator = 9;
  EXPECT_EQ(s.render_header(), "<<<df:sep(9)>>>\n");
}

TEST(CheckMkSection, RenderHeaderWithAllDecorations) {
  packet::section s("mysection");
  s.separator = 124;
  s.cached = std::make_pair(1500000000LL, 120LL);
  s.persist_until = 1600000000LL;
  EXPECT_EQ(s.render_header(), "<<<mysection:sep(124):cached(1500000000,120):persist(1600000000)>>>\n");
}

TEST(CheckMkSection, ToStringUsesSeparatorForLines) {
  packet::section s("df");
  s.separator = '|';
  s.push("a b c");
  EXPECT_EQ(s.to_string(), "<<<df:sep(124)>>>\na|b|c\n");
}

TEST(CheckMkSection, ToStringDefaultsToSpaceSeparator) {
  packet::section s("df");
  s.push("a b");
  s.push("c d");
  EXPECT_EQ(s.to_string(), "<<<df>>>\na b\nc d\n");
}

TEST(CheckMkSection, EmptyReflectsTitleAndLines) {
  packet::section s;
  EXPECT_TRUE(s.empty());
  s.title = "x";
  EXPECT_FALSE(s.empty());
  packet::section s2;
  s2.push("data");
  EXPECT_FALSE(s2.empty());
}

TEST(CheckMkSection, GetLineReturnsRequestedLine) {
  packet::section s("t");
  s.push("first line");
  s.push("second line");
  EXPECT_EQ(s.get_line(1).get_line(), "second line");
}

TEST(CheckMkSection, GetLineOutOfRangeThrows) {
  packet::section s("t");
  s.push("only");
  EXPECT_THROW(s.get_line(1), check_mk::check_mk_exception);
}

TEST(CheckMkSection, AddLineAppends) {
  packet::section s("t");
  packet::section::line l;
  l.set_raw("raw data");
  s.add_line(l);
  ASSERT_EQ(s.lines.size(), 1u);
  EXPECT_EQ(s.get_line(0).get_line(), "raw data");
}

TEST(CheckMkSection, CopyAndAssignPreserveDecorations) {
  packet::section s("t");
  s.separator = 9;
  s.cached = std::make_pair(1LL, 2LL);
  s.persist_until = 3LL;
  s.push("a");
  packet::section copy(s);
  packet::section assigned;
  assigned = s;
  EXPECT_EQ(copy.to_string(), s.to_string());
  EXPECT_EQ(assigned.to_string(), s.to_string());
}

// =============================================================================
// parse_section_header
// =============================================================================

TEST(CheckMkParseHeader, TitleOnly) {
  packet::section s = packet::parse_section_header("check_mk");
  EXPECT_EQ(s.title, "check_mk");
  EXPECT_FALSE(s.separator);
  EXPECT_FALSE(s.cached);
  EXPECT_FALSE(s.persist_until);
}

TEST(CheckMkParseHeader, SeparatorOption) {
  packet::section s = packet::parse_section_header("df:sep(9)");
  EXPECT_EQ(s.title, "df");
  ASSERT_TRUE(s.separator);
  EXPECT_EQ(*s.separator, 9);
}

TEST(CheckMkParseHeader, CachedOption) {
  packet::section s = packet::parse_section_header("df:cached(1500000000,120)");
  ASSERT_TRUE(s.cached);
  EXPECT_EQ(s.cached->first, 1500000000LL);
  EXPECT_EQ(s.cached->second, 120LL);
}

TEST(CheckMkParseHeader, PersistOption) {
  packet::section s = packet::parse_section_header("df:persist(1600000000)");
  ASSERT_TRUE(s.persist_until);
  EXPECT_EQ(*s.persist_until, 1600000000LL);
}

TEST(CheckMkParseHeader, AllOptionsCombined) {
  packet::section s = packet::parse_section_header("df:sep(124):cached(10,20):persist(30)");
  EXPECT_EQ(s.title, "df");
  ASSERT_TRUE(s.separator);
  EXPECT_EQ(*s.separator, 124);
  ASSERT_TRUE(s.cached);
  EXPECT_EQ(s.cached->first, 10LL);
  EXPECT_EQ(s.cached->second, 20LL);
  ASSERT_TRUE(s.persist_until);
  EXPECT_EQ(*s.persist_until, 30LL);
}

TEST(CheckMkParseHeader, MalformedNumberIsIgnored) {
  packet::section s = packet::parse_section_header("df:sep(bogus):persist(x)");
  EXPECT_EQ(s.title, "df");
  EXPECT_FALSE(s.separator);
  EXPECT_FALSE(s.persist_until);
}

TEST(CheckMkParseHeader, CachedWithoutCommaIsIgnored) {
  packet::section s = packet::parse_section_header("df:cached(100)");
  EXPECT_FALSE(s.cached);
}

TEST(CheckMkParseHeader, UnknownOptionIsIgnored) {
  packet::section s = packet::parse_section_header("df:frobnicate(1)");
  EXPECT_EQ(s.title, "df");
  EXPECT_FALSE(s.separator);
}

// =============================================================================
// packet — write
// =============================================================================

TEST(CheckMkPacket, WriteRendersAllSections) {
  packet p;
  packet::section s1("check_mk");
  s1.push("Version: 1.2.3");
  packet::section s2("local");
  s2.push("0 my_check - OK");
  p.add_section(s1);
  p.add_section(s2);
  EXPECT_EQ(p.write(), "<<<check_mk>>>\nVersion: 1.2.3\n<<<local>>>\n0 my_check - OK\n");
}

TEST(CheckMkPacket, WriteRendersPiggybackBlocks) {
  packet p;
  packet::section s("check_mk");
  s.push("Version: 1.0");
  p.add_section(s);

  packet::section ps("uptime");
  ps.push("12345");
  p.piggyback_for("otherhost").add_section(ps);

  EXPECT_EQ(p.write(),
            "<<<check_mk>>>\n"
            "Version: 1.0\n"
            "<<<<otherhost>>>>\n"
            "<<<uptime>>>\n"
            "12345\n"
            "<<<<>>>>\n");
}

TEST(CheckMkPacket, PiggybackForReusesExistingBlock) {
  packet p;
  packet::piggyback_block &a = p.piggyback_for("host1");
  a.add_section(packet::section("s1"));
  packet::piggyback_block &b = p.piggyback_for("host1");
  b.add_section(packet::section("s2"));
  ASSERT_EQ(p.piggybacks.size(), 1u);
  EXPECT_EQ(p.piggybacks.front().section_list.size(), 2u);
}

TEST(CheckMkPacket, ToVectorMatchesToString) {
  packet p;
  packet::section s("t");
  s.push("x");
  p.add_section(s);
  std::vector<char> v = p.to_vector();
  EXPECT_EQ(std::string(v.begin(), v.end()), p.to_string());
}

// =============================================================================
// packet — read (parsing)
// =============================================================================

TEST(CheckMkPacket, ReadParsesSectionsAndLines) {
  packet p;
  p.read("<<<check_mk>>>\nVersion: 1.2.3\nAgentOS: linux\n<<<local>>>\n0 c - OK\n");
  ASSERT_EQ(p.section_list.size(), 2u);
  packet::section s0 = p.get_section(0);
  EXPECT_EQ(s0.title, "check_mk");
  ASSERT_EQ(s0.lines.size(), 2u);
  EXPECT_EQ(s0.get_line(0).get_line(), "Version: 1.2.3");
  packet::section s1 = p.get_section(1);
  EXPECT_EQ(s1.title, "local");
  EXPECT_EQ(s1.get_line(0).get_item(3), "OK");
}

TEST(CheckMkPacket, ReadStripsCarriageReturns) {
  packet p;
  p.read("<<<check_mk>>>\r\nVersion: 1.0\r\n");
  ASSERT_EQ(p.section_list.size(), 1u);
  EXPECT_EQ(p.get_section(0).title, "check_mk");
  EXPECT_EQ(p.get_section(0).get_line(0).get_line(), "Version: 1.0");
}

TEST(CheckMkPacket, ReadParsesHeaderDecorations) {
  packet p;
  p.read("<<<df:sep(9):cached(100,60)>>>\ndata\n");
  packet::section s = p.get_section(0);
  EXPECT_EQ(s.title, "df");
  ASSERT_TRUE(s.separator);
  EXPECT_EQ(*s.separator, 9);
  ASSERT_TRUE(s.cached);
  EXPECT_EQ(s.cached->first, 100LL);
  EXPECT_EQ(s.cached->second, 60LL);
}

TEST(CheckMkPacket, ReadParsesPiggybackBlocks) {
  packet p;
  p.read(
      "<<<check_mk>>>\n"
      "Version: 1.0\n"
      "<<<<otherhost>>>>\n"
      "<<<uptime>>>\n"
      "12345\n"
      "<<<<>>>>\n"
      "<<<local>>>\n"
      "after piggyback\n");
  ASSERT_EQ(p.section_list.size(), 2u);
  EXPECT_EQ(p.get_section(0).title, "check_mk");
  EXPECT_EQ(p.get_section(1).title, "local");
  ASSERT_EQ(p.piggybacks.size(), 1u);
  EXPECT_EQ(p.piggybacks.front().host, "otherhost");
  ASSERT_EQ(p.piggybacks.front().section_list.size(), 1u);
  EXPECT_EQ(p.piggybacks.front().section_list.front().title, "uptime");
  EXPECT_EQ(p.piggybacks.front().section_list.front().lines.front().get_line(), "12345");
}

TEST(CheckMkPacket, ReadFlushesTrailingSectionWithoutNewline) {
  packet p;
  p.read("<<<t>>>\nlast line");
  ASSERT_EQ(p.section_list.size(), 1u);
  EXPECT_EQ(p.get_section(0).get_line(0).get_line(), "last line");
}

TEST(CheckMkPacket, ReadWriteRoundTrip) {
  const std::string wire =
      "<<<check_mk>>>\n"
      "Version: 1.2.3\n"
      "<<<df:sep(124)>>>\n"
      "root|/|100\n"
      "<<<<piggy>>>>\n"
      "<<<mem>>>\n"
      "total 4096\n"
      "<<<<>>>>\n";
  packet p;
  p.read(wire);
  EXPECT_EQ(p.write(), wire);
}

TEST(CheckMkPacket, GetSectionOutOfRangeThrows) {
  packet p;
  p.read("<<<a>>>\nx\n");
  EXPECT_THROW(p.get_section(1), check_mk::check_mk_exception);
}

TEST(CheckMkPacket, CopyAndAssign) {
  packet p;
  packet::section s("t");
  s.push("x");
  p.add_section(s);
  p.piggyback_for("h").add_section(s);
  packet copy(p);
  packet assigned;
  assigned = p;
  EXPECT_EQ(copy.write(), p.write());
  EXPECT_EQ(assigned.write(), p.write());
}

// =============================================================================
// exception
// =============================================================================

TEST(CheckMkException, WhatReturnsMessage) {
  check_mk::check_mk_exception e("boom");
  EXPECT_STREQ(e.what(), "boom");
}
