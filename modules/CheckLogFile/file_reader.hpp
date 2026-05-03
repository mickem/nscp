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

#pragma once

#include <cstdint>
#include <istream>
#include <sstream>
#include <string>

namespace check_logfile {
namespace file_reader {

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
    if (out.size() >= delim.size() &&
        out.compare(out.size() - delim.size(), delim.size(), delim) == 0) {
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

}  // namespace file_reader
}  // namespace check_logfile
