// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "bookmark_state.hpp"

#include <gtest/gtest.h>

#include <string>

using check_logfile::bookmark::compute_resume;
using check_logfile::bookmark::fnv1a;
using check_logfile::bookmark::format;
using check_logfile::bookmark::parse;
using check_logfile::bookmark::position;
using check_logfile::bookmark::resume_decision;
using check_logfile::bookmark::store;
using check_logfile::bookmark::to_hex;

namespace {
std::uint64_t hash_of(const std::string &s) { return fnv1a(s.data(), s.size()); }
}  // namespace

// ---------------------------------------------------------------------------
// fnv1a
// ---------------------------------------------------------------------------

TEST(bookmark_fnv1a, is_stable_and_known) {
  // Reference values for FNV-1a/64; a change here means stored bookmarks from
  // an older build would suddenly look like a rotated file.
  EXPECT_EQ(14695981039346656037ULL, fnv1a("", 0));
  EXPECT_EQ(0xaf63dc4c8601ec8cULL, hash_of("a"));
  EXPECT_EQ(0x85944171f73967e8ULL, hash_of("foobar"));
}

TEST(bookmark_fnv1a, differs_for_different_content) { EXPECT_NE(hash_of("line one\n"), hash_of("line two\n")); }

// ---------------------------------------------------------------------------
// format / parse
// ---------------------------------------------------------------------------

TEST(bookmark_format, roundtrips) {
  const position pos(4711, 256, 1234567890123456789ULL);
  const std::string s = format(pos);
  EXPECT_EQ("1|4711|256|1234567890123456789", s);
  EXPECT_EQ(pos, parse(s));
}

TEST(bookmark_format, invalid_position_formats_empty) { EXPECT_EQ("", format(position())); }

TEST(bookmark_parse, rejects_garbage) {
  EXPECT_FALSE(parse("").valid);
  EXPECT_FALSE(parse("garbage").valid);
  EXPECT_FALSE(parse("1|1|2").valid);
  EXPECT_FALSE(parse("1|1|2|3|4").valid);
  // Unknown version: discarded rather than mis-read.
  EXPECT_FALSE(parse("2|1|2|3").valid);
  // A negative offset must never wrap into a huge unsigned one, which would
  // silently skip the whole file forever.
  EXPECT_FALSE(parse("1|-1|2|3").valid);
  EXPECT_FALSE(parse("1|1|2|abc").valid);
  EXPECT_FALSE(parse("1| 1|2|3").valid);
  EXPECT_FALSE(parse("1||2|3").valid);
}

// ---------------------------------------------------------------------------
// compute_resume
// ---------------------------------------------------------------------------

TEST(bookmark_resume, first_observation_reads_everything) {
  const resume_decision d = compute_resume(position(), 100, 0, true);
  EXPECT_FALSE(d.skip);
  EXPECT_EQ(0u, d.offset);
  EXPECT_FALSE(d.restarted);
}

TEST(bookmark_resume, empty_file_is_skipped) {
  const resume_decision d = compute_resume(position(), 0, 0, true);
  EXPECT_TRUE(d.skip);
  EXPECT_EQ(0u, d.offset);
}

TEST(bookmark_resume, unchanged_file_is_skipped) {
  const position prev(100, 100, 42);
  const resume_decision d = compute_resume(prev, 100, 42, true);
  EXPECT_TRUE(d.skip);
  EXPECT_EQ(100u, d.offset);
  EXPECT_FALSE(d.restarted);
}

TEST(bookmark_resume, grown_file_resumes_at_offset) {
  const position prev(100, 100, 42);
  const resume_decision d = compute_resume(prev, 250, 42, true);
  EXPECT_FALSE(d.skip);
  EXPECT_EQ(100u, d.offset);
  EXPECT_FALSE(d.restarted);
}

// The offset trails a partially written last line, so a file can be larger
// than the offset and still resume mid-file rather than restart.
TEST(bookmark_resume, partial_tail_is_reread) {
  const position prev(80, 100, 42);
  const resume_decision d = compute_resume(prev, 100, 42, true);
  EXPECT_FALSE(d.skip);
  EXPECT_EQ(80u, d.offset);
}

TEST(bookmark_resume, truncated_file_restarts) {
  const position prev(1000, 256, 42);
  const resume_decision d = compute_resume(prev, 500, 42, true);
  EXPECT_FALSE(d.skip);
  EXPECT_EQ(0u, d.offset);
  EXPECT_TRUE(d.restarted);
}

// The rotation which a size check alone cannot see: the file was replaced by a
// brand new one which is already bigger than where we stopped reading.
TEST(bookmark_resume, replaced_file_restarts_even_when_larger) {
  const position prev(1000, 256, 42);
  const resume_decision d = compute_resume(prev, 5000, 43, true);
  EXPECT_FALSE(d.skip);
  EXPECT_EQ(0u, d.offset);
  EXPECT_TRUE(d.restarted);
}

TEST(bookmark_resume, unreadable_fingerprint_restarts) {
  const position prev(1000, 256, 42);
  const resume_decision d = compute_resume(prev, 2000, 0, false);
  EXPECT_FALSE(d.skip);
  EXPECT_EQ(0u, d.offset);
  EXPECT_TRUE(d.restarted);
}

// A file which was empty last time has no fingerprint to compare against, so
// it must resume on content alone instead of looking permanently rotated.
TEST(bookmark_resume, empty_previous_state_resumes_without_fingerprint) {
  const position prev(0, 0, 0);
  const resume_decision d = compute_resume(prev, 120, 0, true);
  EXPECT_FALSE(d.skip);
  EXPECT_EQ(0u, d.offset);
  EXPECT_FALSE(d.restarted);
}

TEST(bookmark_resume, emptied_file_drops_the_stored_offset) {
  const position prev(1000, 256, 42);
  const resume_decision d = compute_resume(prev, 0, 42, true);
  EXPECT_TRUE(d.skip);
  EXPECT_EQ(0u, d.offset);
  EXPECT_TRUE(d.restarted);
}

// ---------------------------------------------------------------------------
// to_hex
// ---------------------------------------------------------------------------

TEST(bookmark_to_hex, is_fixed_width_and_lowercase) {
  EXPECT_EQ("0000000000000000", to_hex(0));
  EXPECT_EQ("00000000000000ff", to_hex(255));
  EXPECT_EQ("ffffffffffffffff", to_hex(0xffffffffffffffffULL));
  EXPECT_EQ("123456789abcdef0", to_hex(0x123456789abcdef0ULL));
}

// ---------------------------------------------------------------------------
// store (the bounded position map)
// ---------------------------------------------------------------------------

TEST(bookmark_store, stores_and_returns_values) {
  store s;
  EXPECT_EQ("", s.get("nothing"));
  s.put("a", "1|1|0|0");
  EXPECT_EQ("1|1|0|0", s.get("a"));
  s.put("a", "1|2|0|0");
  EXPECT_EQ("1|2|0|0", s.get("a"));
  EXPECT_EQ(1u, s.size());
}

TEST(bookmark_store, snapshot_returns_every_entry) {
  store s;
  s.put("a", "1");
  s.put("b", "2");
  const store::map_type all = s.snapshot();
  ASSERT_EQ(2u, all.size());
  EXPECT_EQ("1", all.find("a")->second);
  EXPECT_EQ("2", all.find("b")->second);
}

// A caller which invents a new bookmark name on every check must not be able
// to grow the state (and thus nsclient.db) without bound.
TEST(bookmark_store, evicts_the_least_recently_used_entry) {
  store s(3);
  s.put("a", "1");
  s.put("b", "2");
  s.put("c", "3");
  s.put("d", "4");
  EXPECT_EQ(3u, s.size());
  // "a" was the oldest, so it is the one which had to go.
  EXPECT_EQ("", s.get("a"));
  EXPECT_EQ("2", s.get("b"));
  EXPECT_EQ("4", s.get("d"));
}

TEST(bookmark_store, reading_a_position_keeps_it_alive) {
  store s(2);
  s.put("a", "1");
  s.put("b", "2");
  // A quiet check still reads its position - that has to count as a use, or a
  // bookmark with nothing new to report would age out before a noisy one.
  EXPECT_EQ("1", s.get("a"));
  s.put("c", "3");
  EXPECT_EQ("1", s.get("a"));
  EXPECT_EQ("", s.get("b"));
}

TEST(bookmark_store, updating_a_position_keeps_it_alive) {
  store s(2);
  s.put("a", "1");
  s.put("b", "2");
  s.put("a", "1b");
  s.put("c", "3");
  EXPECT_EQ("1b", s.get("a"));
  EXPECT_EQ("", s.get("b"));
}

TEST(bookmark_store, a_zero_limit_still_holds_one_entry) {
  store s(0);
  s.put("a", "1");
  EXPECT_EQ("1", s.get("a"));
  s.put("b", "2");
  EXPECT_EQ(1u, s.size());
  EXPECT_EQ("2", s.get("b"));
}
