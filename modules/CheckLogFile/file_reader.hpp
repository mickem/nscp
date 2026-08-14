// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <cstdint>
#include <istream>
#include <sstream>
#include <string>

namespace check_logfile {
namespace file_reader {

// How much is read at a time when only a bounded number of records is wanted.
const std::size_t record_scan_chunk_size = 64 * 1024;

// Read the next chunk from `is` delimited by `delim` into `out`.
//
// Behavior is intentionally close to `std::getline(is, out, ch)` but works
// with multi-character delimiters as well, which is what the documented
// `line-split` argument requires.
//
//   * The delimiter itself is NOT included in `out`.
//   * On Windows, `std::getline` strips a trailing CR from a CRLF line in
//     text mode. To get equivalent behavior portably (and to keep prior
//     behavior on POSIX where files may carry CRLF endings), if the
//     delimiter ends in '\n' a trailing '\r' is stripped from `out`.
//   * If `delim` is empty the entire remaining stream is read into `out`
//     as a single chunk (useful when the user wants to treat the whole
//     file as one record and use a multi-line regexp).
//   * Returns true if a chunk was extracted (possibly empty if the
//     delimiter matched immediately). Returns false when there is
//     nothing left to read.
inline bool getline_str(std::istream &is, std::string &out, const std::string &delim) {
  out.clear();

  if (delim.empty()) {
    std::ostringstream ss;
    ss << is.rdbuf();
    out = ss.str();
    return !out.empty();
  }

  if (delim.size() == 1) {
    char c;
    bool any = false;
    while (is.get(c)) {
      any = true;
      if (c == delim[0]) {
        // Strip trailing CR for "\n" delimiter to mimic text-mode getline.
        if (delim[0] == '\n' && !out.empty() && out.back() == '\r') {
          out.pop_back();
        }
        return true;
      }
      out.push_back(c);
    }
    return any;
  }

  // Multi-character delimiter: scan char-by-char and check whether the
  // accumulated buffer ends with `delim`. This is correct even for
  // self-overlapping patterns because we always re-test the suffix.
  char c;
  bool any = false;
  while (is.get(c)) {
    any = true;
    out.push_back(c);
    if (out.size() >= delim.size() && out.compare(out.size() - delim.size(), delim.size(), delim) == 0) {
      out.resize(out.size() - delim.size());
      // Same CRLF-trim convenience as the single-char case.
      if (delim.back() == '\n' && !out.empty() && out.back() == '\r') {
        out.pop_back();
      }
      return true;
    }
  }
  return any;
}

// Decision on where to start reading a file in the real-time logfile path.
//
// `prev_size` is the size of the file the last time it was processed
// (0 on the very first observation). `cur_size` is the current size on
// disk. `read_from_start` mirrors the `read entire file` setting.
struct seek_decision {
  // When true, do not read any data this round (no new content).
  bool skip;
  // Byte offset to seek to in the file before reading.
  // 0 means "read from the beginning"; any other value means
  // "resume after the previously processed tail".
  std::uint64_t offset;

  bool operator==(const seek_decision &o) const { return skip == o.skip && offset == o.offset; }
};

inline seek_decision compute_seek(std::uint64_t prev_size, std::uint64_t cur_size, bool read_from_start) {
  // Caller asked for the entire file every time.
  if (read_from_start) {
    if (cur_size == 0) return {true, 0};
    return {false, 0};
  }
  // Empty file: nothing to do.
  if (cur_size == 0) return {true, 0};
  // File shrank (rotated/truncated) - the previous offset is no longer
  // meaningful, so start from the beginning. This matches what users
  // expect after a logrotate / file rewrite.
  if (cur_size < prev_size) return {false, 0};
  // No new data since last time.
  if (cur_size == prev_size) return {true, prev_size};
  // Resume reading after the last read tail.
  return {false, prev_size};
}

// True when two occurrences of `delim` can overlap, i.e. when a proper prefix
// of it is also a suffix (`aaa`, `abab`, `|||`).
//
// Records are split by a left-to-right scan which consumes each delimiter
// whole, so for such a delimiter "where does an occurrence start" cannot be
// answered without the bytes in front of it - `aaaa` split on `aaa` is one
// delimiter at offset 0 and a record `a`, not a delimiter at offset 1. A scan
// which starts at the end has no way to know, so it must not be used.
inline bool delim_can_overlap(const std::string &delim) {
  for (std::size_t k = 1; k < delim.size(); k++) {
    if (delim.compare(0, k, delim, delim.size() - k, k) == 0) return true;
  }
  return false;
}

// Byte offset at which the last `max_records` records of `is` begin.
//
// A record is a chunk terminated by `delim`; a trailing chunk which is not
// terminated counts as a record too, since that is how the check treats it
// when no bookmark is in play. Returns 0 when the stream holds no more than
// `max_records` records, when there is nothing to limit (no delimiter, no
// limit), and for a self-overlapping delimiter, which cannot be located from
// the end (see delim_can_overlap). 0 is always a safe answer: it only means
// the caller reads more than it strictly needs and drops the surplus records
// itself.
//
// The scan runs backwards from the end so that `max-lines` bounds the I/O and
// not only the matching: asking for the last 10 lines of a multi-gigabyte log
// should read a few kilobytes, not the whole file.
//
// The stream is left positioned wherever the scan ended and its flags are
// cleared; every caller seeks afterwards.
inline std::uint64_t find_tail_offset(std::istream &is, const std::string &delim, std::uint64_t max_records) {
  if (max_records == 0 || delim.empty() || delim_can_overlap(delim)) return 0;
  is.clear();
  is.seekg(0, std::ios::end);
  const std::streamoff end_pos = is.tellg();
  if (end_pos <= 0) return 0;
  const std::uint64_t size = static_cast<std::uint64_t>(end_pos);
  if (size < delim.size()) return 0;

  // A stream ending with the delimiter has no trailing partial record, so the
  // start of the last `max_records` records is one delimiter further back.
  std::string tail(delim.size(), '\0');
  is.seekg(static_cast<std::streamoff>(size - delim.size()), std::ios::beg);
  is.read(&tail[0], static_cast<std::streamsize>(delim.size()));
  tail.resize(static_cast<std::size_t>(is.gcount()));
  is.clear();
  const std::uint64_t wanted = max_records + (tail == delim ? 1 : 0);

  // Keep the last `delim.size() - 1` bytes of the region already scanned in
  // front of the next chunk, so a delimiter straddling a chunk boundary is
  // still found. A match cannot start inside that carry (it would not fit),
  // so nothing is counted twice.
  const std::size_t overlap = delim.size() - 1;
  std::string carry;
  std::uint64_t pos = size;
  std::uint64_t found = 0;
  while (pos > 0) {
    const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(record_scan_chunk_size, pos));
    const std::uint64_t start = pos - want;
    std::string buf;
    buf.resize(want);
    is.seekg(static_cast<std::streamoff>(start), std::ios::beg);
    is.read(&buf[0], static_cast<std::streamsize>(want));
    buf.resize(static_cast<std::size_t>(is.gcount()));
    is.clear();
    buf += carry;

    std::string::size_type search_to = buf.size();
    while (true) {
      const std::string::size_type hit = buf.rfind(delim, search_to);
      if (hit == std::string::npos) break;
      if (++found == wanted) return start + hit + delim.size();
      if (hit == 0) break;
      search_to = hit - 1;
    }

    carry = buf.substr(0, std::min(overlap, buf.size()));
    pos = start;
  }
  return 0;
}

// Read from the current position of `is` until `max_records` complete records
// have been read - i.e. up to and including the delimiter which terminates the
// last of them - or the stream ends, whichever comes first.
//
// The counterpart of find_tail_offset for files whose newest record is at the
// top: the caller wants the leading records, so reading stops as soon as it has
// them instead of pulling in the rest of the file.
inline std::string read_leading_records(std::istream &is, const std::string &delim, std::uint64_t max_records) {
  std::string out;
  if (delim.empty() || max_records == 0) {
    std::ostringstream ss;
    ss << is.rdbuf();
    return ss.str();
  }

  std::uint64_t found = 0;
  // Everything before this has already been searched; it only ever moves
  // forward, so a delimiter is never matched twice (which matters for
  // self-overlapping delimiters such as `aaa`).
  std::string::size_type search_from = 0;
  while (is) {
    const std::string::size_type before = out.size();
    out.resize(before + record_scan_chunk_size);
    is.read(&out[before], static_cast<std::streamsize>(record_scan_chunk_size));
    const std::size_t got = static_cast<std::size_t>(is.gcount());
    out.resize(before + got);
    is.clear();
    if (got == 0) break;

    while (true) {
      const std::string::size_type hit = out.find(delim, search_from);
      if (hit == std::string::npos) break;
      search_from = hit + delim.size();
      if (++found == max_records) {
        out.resize(search_from);
        return out;
      }
    }
    // No match in what has been read so far: only a delimiter straddling the
    // end of the buffer can still be completed, so resume from there.
    const std::string::size_type scanned = out.size() >= delim.size() ? out.size() - delim.size() + 1 : 0;
    if (search_from < scanned) search_from = scanned;
  }
  return out;
}

}  // namespace file_reader
}  // namespace check_logfile
