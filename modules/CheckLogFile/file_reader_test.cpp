// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "file_reader.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

using check_logfile::file_reader::compute_seek;
using check_logfile::file_reader::delim_can_overlap;
using check_logfile::file_reader::find_tail_offset;
using check_logfile::file_reader::getline_str;
using check_logfile::file_reader::read_leading_records;
using check_logfile::file_reader::record_scan_chunk_size;
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

std::uint64_t tail_offset(const std::string &input, const std::string &delim, std::uint64_t max_records) {
  std::istringstream is(input);
  return find_tail_offset(is, delim, max_records);
}

std::string leading(const std::string &input, const std::string &delim, std::uint64_t max_records) {
  std::istringstream is(input);
  return read_leading_records(is, delim, max_records);
}

// Split exactly the way CheckLogFile::match_records does, so a test can assert
// that an offset really is a record boundary.
std::vector<std::string> split_records(const std::string &input, const std::string &delim) {
  std::vector<std::string> out;
  std::string::size_type pos = 0, lpos = 0;
  while ((pos = input.find(delim, pos)) != std::string::npos) {
    out.push_back(input.substr(lpos, pos - lpos));
    pos += delim.size();
    lpos = pos;
  }
  if (lpos < input.size()) out.push_back(input.substr(lpos));
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
// delim_can_overlap: which delimiters can be located from the end (issue #583)
// ---------------------------------------------------------------------------

TEST(file_reader_delim_can_overlap, ordinary_delimiters_cannot_overlap) {
  EXPECT_FALSE(delim_can_overlap("\n"));
  EXPECT_FALSE(delim_can_overlap("\r\n"));
  EXPECT_FALSE(delim_can_overlap("<END>"));
  EXPECT_FALSE(delim_can_overlap("-|"));
  EXPECT_FALSE(delim_can_overlap(""));
}

TEST(file_reader_delim_can_overlap, self_overlapping_delimiters_are_detected) {
  EXPECT_TRUE(delim_can_overlap("aaa"));
  // "---" is one delimiter and a leftover "-" going forwards, but looks like a
  // delimiter at offset 1 going backwards.
  EXPECT_TRUE(delim_can_overlap("--"));
  EXPECT_TRUE(delim_can_overlap("abab"));
  EXPECT_TRUE(delim_can_overlap("|||"));
  EXPECT_TRUE(delim_can_overlap("\r\n\r\n"));
}

// ---------------------------------------------------------------------------
// find_tail_offset: where the newest N records start (issue #583)
// ---------------------------------------------------------------------------

TEST(file_reader_find_tail_offset, trailing_delimiter_does_not_count_as_a_record) {
  // "a\nb\nc\n" is three records, not four: the offsets are 0, 2 and 4.
  EXPECT_EQ(4u, tail_offset("a\nb\nc\n", "\n", 1));
  EXPECT_EQ(2u, tail_offset("a\nb\nc\n", "\n", 2));
  EXPECT_EQ(0u, tail_offset("a\nb\nc\n", "\n", 3));
}

TEST(file_reader_find_tail_offset, unterminated_final_record_counts) {
  EXPECT_EQ(4u, tail_offset("a\nb\nc", "\n", 1));
  EXPECT_EQ(2u, tail_offset("a\nb\nc", "\n", 2));
  EXPECT_EQ(0u, tail_offset("a\nb\nc", "\n", 3));
}

TEST(file_reader_find_tail_offset, fewer_records_than_asked_for_reads_everything) {
  EXPECT_EQ(0u, tail_offset("a\nb\n", "\n", 10));
  EXPECT_EQ(0u, tail_offset("no delimiter here", "\n", 1));
  EXPECT_EQ(0u, tail_offset("", "\n", 1));
}

TEST(file_reader_find_tail_offset, no_limit_or_no_delimiter_reads_everything) {
  EXPECT_EQ(0u, tail_offset("a\nb\nc\n", "\n", 0));
  EXPECT_EQ(0u, tail_offset("a\nb\nc\n", "", 1));
}

TEST(file_reader_find_tail_offset, multi_char_delimiter) {
  const std::string input = "a\r\nb\r\nc\r\n";
  EXPECT_EQ(6u, tail_offset(input, "\r\n", 1));
  EXPECT_EQ(3u, tail_offset(input, "\r\n", 2));
  EXPECT_EQ(0u, tail_offset(input, "\r\n", 3));
}

TEST(file_reader_find_tail_offset, self_overlapping_delimiter_falls_back_to_the_whole_file) {
  // A backwards scan cannot tell whether the delimiter in "xaaaay" starts at
  // index 1 or 2, so it declines and lets the caller read everything.
  EXPECT_EQ(0u, tail_offset("xaaaay", "aaa", 1));
}

TEST(file_reader_find_tail_offset, offset_is_a_record_boundary_across_chunks) {
  // Enough lines to span several 64 KiB scan chunks.
  std::string input;
  std::vector<std::string> expected;
  for (int i = 0; i < 20000; i++) {
    const std::string line = "2018-07-20 12:00:00 line " + std::to_string(i);
    expected.push_back(line);
    input += line + "\n";
  }
  ASSERT_GT(input.size(), 3u * record_scan_chunk_size);

  const std::uint64_t offset = tail_offset(input, "\n", 3);
  const std::vector<std::string> tail = split_records(input.substr(static_cast<std::size_t>(offset)), "\n");
  ASSERT_EQ(3u, tail.size());
  EXPECT_EQ(expected[19997], tail[0]);
  EXPECT_EQ(expected[19998], tail[1]);
  EXPECT_EQ(expected[19999], tail[2]);
}

TEST(file_reader_find_tail_offset, delimiter_straddling_a_chunk_boundary) {
  // Chunks are taken from the end of the file, so the boundary of the first
  // one is at size - 64 KiB. Place a "\r\n" so that the '\r' is the last byte
  // of the second chunk and the '\n' the first byte of the first one: only the
  // carried-over overlap makes it findable.
  const std::size_t tail_len = record_scan_chunk_size - 1;
  const std::string input = std::string(3 * record_scan_chunk_size, 'x') + "\r\n" + std::string(tail_len, 'y');
  ASSERT_EQ('\r', input[input.size() - tail_len - 2]);

  const std::uint64_t offset = tail_offset(input, "\r\n", 1);
  EXPECT_EQ(input.size() - tail_len, offset);
  EXPECT_EQ(std::string(tail_len, 'y'), input.substr(static_cast<std::size_t>(offset)));
}

// ---------------------------------------------------------------------------
// read_leading_records: the first N records, for files written newest-first
// ---------------------------------------------------------------------------

TEST(file_reader_read_leading_records, stops_after_the_nth_delimiter) {
  EXPECT_EQ("a\nb\n", leading("a\nb\nc\nd\n", "\n", 2));
  EXPECT_EQ("a\n", leading("a\nb\nc\nd\n", "\n", 1));
}

TEST(file_reader_read_leading_records, fewer_records_than_asked_for_returns_everything) {
  EXPECT_EQ("a\nb\nc", leading("a\nb\nc", "\n", 5));
  EXPECT_EQ("", leading("", "\n", 5));
}

TEST(file_reader_read_leading_records, no_limit_or_no_delimiter_returns_everything) {
  EXPECT_EQ("a\nb\nc\n", leading("a\nb\nc\n", "\n", 0));
  EXPECT_EQ("a\nb\nc\n", leading("a\nb\nc\n", "", 3));
}

TEST(file_reader_read_leading_records, multi_char_delimiter) { EXPECT_EQ("alpha||beta||", leading("alpha||beta||gamma", "||", 2)); }

TEST(file_reader_read_leading_records, self_overlapping_delimiter_splits_like_match_records) {
  // The forward scan consumes each delimiter whole, exactly as the record
  // splitter does: "xaaaay" is the record "x" followed by "ay".
  EXPECT_EQ("xaaa", leading("xaaaay", "aaa", 1));
}

TEST(file_reader_read_leading_records, delimiter_straddling_a_chunk_boundary) {
  const std::string input = std::string(record_scan_chunk_size - 1, 'x') + "\r\n" + std::string(1000, 'y');
  EXPECT_EQ(std::string(record_scan_chunk_size - 1, 'x') + "\r\n", leading(input, "\r\n", 1));
}

TEST(file_reader_read_leading_records, reads_only_what_it_needs) {
  // The point of the helper: a huge file whose first record is short must not
  // be pulled into memory in full.
  const std::string input = "first line\n" + std::string(4 * record_scan_chunk_size, 'x');
  const std::string got = leading(input, "\n", 1);
  EXPECT_EQ("first line\n", got);
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
