/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include "file_reader.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

using check_logfile::file_reader::compute_seek;
using check_logfile::file_reader::getline_str;
using check_logfile::file_reader::seek_decision;

namespace {
std::vector<std::string> read_all(const std::string &input, const std::string &delim) {
  std::istringstream is(input);
  std::vector<std::string> out;
  std::string line;
  while (getline_str(is, line, delim)) {
    out.push_back(line);
  }
  return out;
}
}  // namespace

// ---------------------------------------------------------------------------
// getline_str: single-character delimiter (matches default \n behavior)
// ---------------------------------------------------------------------------

TEST(file_reader_getline_str, single_char_basic) {
  auto v = read_all("a\nb\nc\n", "\n");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("a", v[0]);
  EXPECT_EQ("b", v[1]);
  EXPECT_EQ("c", v[2]);
}

TEST(file_reader_getline_str, single_char_no_trailing_delimiter) {
  // Last line has no terminator - it must still be returned.
  auto v = read_all("a\nb\nc", "\n");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("a", v[0]);
  EXPECT_EQ("b", v[1]);
  EXPECT_EQ("c", v[2]);
}

TEST(file_reader_getline_str, single_char_empty_lines) {
  auto v = read_all("\n\nx\n", "\n");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("", v[0]);
  EXPECT_EQ("", v[1]);
  EXPECT_EQ("x", v[2]);
}

TEST(file_reader_getline_str, single_char_strips_trailing_cr_for_lf) {
  // Mimic the text-mode getline behavior on Windows: CRLF input must yield
  // lines without the trailing \r when the delimiter is \n.
  auto v = read_all("a\r\nb\r\nc\r\n", "\n");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("a", v[0]);
  EXPECT_EQ("b", v[1]);
  EXPECT_EQ("c", v[2]);
}

TEST(file_reader_getline_str, single_char_does_not_strip_cr_for_other_delim) {
  // CR-stripping must NOT trigger for arbitrary delimiters - only for ones
  // ending in \n. Here the user picks ';' explicitly.
  auto v = read_all("a\r;b\r;c", ";");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("a\r", v[0]);
  EXPECT_EQ("b\r", v[1]);
  EXPECT_EQ("c", v[2]);
}

TEST(file_reader_getline_str, single_char_empty_input) {
  auto v = read_all("", "\n");
  EXPECT_TRUE(v.empty());
}

// ---------------------------------------------------------------------------
// getline_str: multi-character delimiter (issue #581)
// ---------------------------------------------------------------------------

TEST(file_reader_getline_str, multi_char_crlf) {
  // Explicitly matching CRLF as a record terminator.
  auto v = read_all("a\r\nb\r\nc\r\n", "\r\n");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("a", v[0]);
  EXPECT_EQ("b", v[1]);
  EXPECT_EQ("c", v[2]);
}

TEST(file_reader_getline_str, multi_char_arbitrary_string) {
  auto v = read_all("alpha||beta||gamma", "||");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("alpha", v[0]);
  EXPECT_EQ("beta", v[1]);
  EXPECT_EQ("gamma", v[2]);
}

TEST(file_reader_getline_str, multi_char_self_overlapping) {
  // 'aaa' inside 'aaaa' must produce one match at the first three a's,
  // then leave the trailing 'a' as the final record. The suffix-compare
  // approach handles this correctly.
  auto v = read_all("xaaaay", "aaa");
  ASSERT_EQ(2u, v.size());
  EXPECT_EQ("x", v[0]);
  EXPECT_EQ("ay", v[1]);
}

TEST(file_reader_getline_str, multi_char_no_match_returns_whole_input) {
  auto v = read_all("hello world", "ZZ");
  ASSERT_EQ(1u, v.size());
  EXPECT_EQ("hello world", v[0]);
}

TEST(file_reader_getline_str, multi_char_partial_match_then_real_match) {
  // 'XY' should not be consumed by the partial 'X' that precedes 'XYZ'.
  auto v = read_all("aXXYbXYc", "XY");
  ASSERT_EQ(3u, v.size());
  EXPECT_EQ("aX", v[0]);
  EXPECT_EQ("b", v[1]);
  EXPECT_EQ("c", v[2]);
}

// ---------------------------------------------------------------------------
// getline_str: empty delimiter -> entire stream as one record
// ---------------------------------------------------------------------------

TEST(file_reader_getline_str, empty_delim_returns_whole_input) {
  auto v = read_all("line1\nline2\nline3", "");
  ASSERT_EQ(1u, v.size());
  EXPECT_EQ("line1\nline2\nline3", v[0]);
}

TEST(file_reader_getline_str, empty_delim_empty_input) {
  auto v = read_all("", "");
  EXPECT_TRUE(v.empty());
}

// ---------------------------------------------------------------------------
// compute_seek: real-time read-position logic (issue #582)
// ---------------------------------------------------------------------------

TEST(file_reader_compute_seek, first_observation_reads_from_start) {
  // First time we see a non-empty file, there is no previous offset to
  // resume from. The user expects existing content to be evaluated on the
  // initial scan (issue #582 reports 1 match on the first round); only
  // *subsequent* rounds should be incremental.
  EXPECT_EQ((seek_decision{false, 0u}), compute_seek(0u, 100u, false));
}

TEST(file_reader_compute_seek, appended_data_seeks_to_previous_end) {
  // The actual regression case from #582: after the first scan recorded
  // size=100, a write extending the file to 200 must produce a seek to
  // offset 100 (not to offset 0 nor offset 200).
  EXPECT_EQ((seek_decision{false, 100u}), compute_seek(100u, 200u, false));
}

TEST(file_reader_compute_seek, no_change_skips) { EXPECT_EQ((seek_decision{true, 100u}), compute_seek(100u, 100u, false)); }

TEST(file_reader_compute_seek, truncation_starts_from_zero) {
  // File shrank (rotated/rewritten) - the previous offset is no longer
  // meaningful, so we must read from the beginning.
  EXPECT_EQ((seek_decision{false, 0u}), compute_seek(500u, 200u, false));
}

TEST(file_reader_compute_seek, empty_file_skips) {
  EXPECT_EQ((seek_decision{true, 0u}), compute_seek(0u, 0u, false));
  EXPECT_EQ((seek_decision{true, 0u}), compute_seek(123u, 0u, false));
}

TEST(file_reader_compute_seek, read_from_start_always_offset_zero) {
  EXPECT_EQ((seek_decision{false, 0u}), compute_seek(0u, 100u, true));
  EXPECT_EQ((seek_decision{false, 0u}), compute_seek(50u, 100u, true));
  EXPECT_EQ((seek_decision{false, 0u}), compute_seek(100u, 100u, true));
  // Even with read_from_start=true, an empty file produces no work.
  EXPECT_EQ((seek_decision{true, 0u}), compute_seek(0u, 0u, true));
}

// ---------------------------------------------------------------------------
// Integration-style: simulate the realtime read loop using getline_str +
// compute_seek against an in-memory buffer to confirm only the new tail
// is parsed after a write.
// ---------------------------------------------------------------------------

TEST(file_reader_integration, only_new_tail_is_read_on_subsequent_call) {
  // Round 1: file contains a single line. First observation -> read
  // from start (matches the historical 1-match-on-first-run behavior
  // referenced in #582).
  std::string content_round1 = "2018-07-20 12:00:01 OK\n";
  auto d1 = compute_seek(0u, content_round1.size(), false);
  ASSERT_FALSE(d1.skip);
  EXPECT_EQ(0u, d1.offset);

  // Round 2: a line is appended. We must resume from the previous end,
  // not re-read the file from the beginning (which is the actual #582 bug).
  std::string content_round2 = content_round1 + "2018-07-20 12:01:02 OK\n";
  auto d2 = compute_seek(content_round1.size(), content_round2.size(), false);
  ASSERT_FALSE(d2.skip);
  EXPECT_EQ(content_round1.size(), d2.offset);

  std::istringstream is(content_round2);
  is.seekg(static_cast<std::streamoff>(d2.offset));
  std::vector<std::string> lines;
  std::string line;
  while (getline_str(is, line, "\n")) {
    if (!line.empty()) lines.push_back(line);
  }
  ASSERT_EQ(1u, lines.size()) << "Expected exactly the single newly-appended line";
  EXPECT_EQ("2018-07-20 12:01:02 OK", lines[0]);
}
