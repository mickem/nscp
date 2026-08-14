// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace check_logfile {
namespace bookmark {

// Number of leading bytes of a log file used to fingerprint it.
//
// The fingerprint is what tells "the file grew" apart from "the file was
// replaced by a new file which happens to be larger than the offset we
// stored" (log rotation with copytruncate, a daily file re-created under the
// same name, ...). A size comparison alone only catches the shrinking case.
const std::size_t max_fingerprint_len = 256;

// 64-bit FNV-1a. Not a security primitive - it only has to be stable across
// runs and platforms and cheap on a few hundred bytes, both of which it is.
inline std::uint64_t fnv1a(const char *data, std::size_t len) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (std::size_t i = 0; i < len; i++) {
    hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(data[i]));
    hash *= 1099511628211ULL;
  }
  return hash;
}

// 16 lowercase hex digits, used to name a bookmark after the expressions it
// belongs to without spelling them out in the key.
inline std::string to_hex(std::uint64_t value) {
  static const char digits[] = "0123456789abcdef";
  std::string ret(16, '0');
  for (std::size_t i = 0; i < 16; i++) {
    ret[15 - i] = digits[value & 0xf];
    value >>= 4;
  }
  return ret;
}

// Where a previous check stopped reading a given file.
//
// `offset` is the end of the last COMPLETE record consumed, not the size of
// the file: a log file whose final line has not been terminated yet must be
// re-read from the start of that partial line so the line is reported once,
// in full, when it is finished.
struct position {
  // False when there is no (usable) stored state, i.e. the file has never
  // been checked under this bookmark before.
  bool valid;
  std::uint64_t offset;
  // Number of leading bytes `fingerprint` was computed over. Stored so the
  // comparison later re-hashes exactly the same byte range - hashing
  // "the first min(size, 256) bytes" on both sides would spuriously differ
  // while a file smaller than 256 bytes is still growing.
  std::uint64_t fingerprint_len;
  std::uint64_t fingerprint;

  position() : valid(false), offset(0), fingerprint_len(0), fingerprint(0) {}
  position(std::uint64_t offset_, std::uint64_t fingerprint_len_, std::uint64_t fingerprint_)
      : valid(true), offset(offset_), fingerprint_len(fingerprint_len_), fingerprint(fingerprint_) {}

  bool operator==(const position &o) const {
    return valid == o.valid && offset == o.offset && fingerprint_len == o.fingerprint_len && fingerprint == o.fingerprint;
  }
};

// Serialized form: "1|<offset>|<fingerprint-len>|<fingerprint>".
// The leading version tag lets a future format change be detected (and the
// state discarded, which merely re-reads a file once) instead of being
// mis-parsed into a bogus offset.
inline std::string format(const position &pos) {
  if (!pos.valid) return "";
  return "1|" + std::to_string(pos.offset) + "|" + std::to_string(pos.fingerprint_len) + "|" + std::to_string(pos.fingerprint);
}

inline position parse(const std::string &data) {
  position ret;
  if (data.empty()) return ret;
  std::vector<std::string> chunks;
  std::string::size_type pos = 0;
  while (true) {
    const std::string::size_type next = data.find('|', pos);
    if (next == std::string::npos) {
      chunks.push_back(data.substr(pos));
      break;
    }
    chunks.push_back(data.substr(pos, next - pos));
    pos = next + 1;
  }
  if (chunks.size() != 4 || chunks[0] != "1") return ret;
  try {
    // Reject anything that is not a plain non-negative number: stoull happily
    // accepts leading whitespace, a sign and trailing garbage, and a negative
    // value would wrap into a huge offset that silently skips the whole file.
    for (std::size_t i = 1; i < chunks.size(); i++) {
      if (chunks[i].empty() || chunks[i].find_first_not_of("0123456789") != std::string::npos) return position();
    }
    ret.offset = std::stoull(chunks[1]);
    ret.fingerprint_len = std::stoull(chunks[2]);
    ret.fingerprint = std::stoull(chunks[3]);
    ret.valid = true;
  } catch (const std::exception &) {
    return position();
  }
  return ret;
}

// What to do with a file given the state stored by the previous check.
struct resume_decision {
  // Nothing new to read: the file is empty or has not grown.
  bool skip;
  // Byte offset to start reading at (0 = from the beginning).
  std::uint64_t offset;
  // The stored state was discarded (first observation, or the file was
  // truncated/rotated/replaced) so the file is read from the start again.
  // Only interesting for logging.
  bool restarted;

  bool operator==(const resume_decision &o) const { return skip == o.skip && offset == o.offset && restarted == o.restarted; }
};

// `head_fingerprint` must be the hash of the first `prev.fingerprint_len`
// bytes of the file as it is now, and `head_valid` false when that many bytes
// could not be read (the file is now shorter than the fingerprint).
inline resume_decision compute_resume(const position &prev, std::uint64_t cur_size, std::uint64_t head_fingerprint, bool head_valid) {
  // Nothing to read at all - and make sure any stored offset is dropped, or a
  // truncated file that grows back would resume in the middle of new content.
  if (cur_size == 0) {
    resume_decision d = {true, 0, prev.valid && prev.offset != 0};
    return d;
  }
  if (!prev.valid) {
    // First time this file is seen under this bookmark: report what is there
    // now, then track incrementally from here on.
    resume_decision d = {false, 0, false};
    return d;
  }
  const bool rotated = cur_size < prev.offset || (prev.fingerprint_len > 0 && (!head_valid || head_fingerprint != prev.fingerprint));
  if (rotated) {
    resume_decision d = {false, 0, true};
    return d;
  }
  if (cur_size == prev.offset) {
    resume_decision d = {true, prev.offset, false};
    return d;
  }
  resume_decision d = {false, prev.offset, false};
  return d;
}

// Upper bound on the number of positions which are remembered (and hence
// persisted to nsclient.db).
//
// A bookmark name comes from whoever runs the query, and an automatic name
// changes whenever the filter does, so nothing stops a caller from creating a
// new name on every check. Without a cap that grows the stored state - and the
// file it is saved to - without bound. 1000 positions is far more than a real
// configuration uses (it is per bookmark AND file) while staying small enough
// to be irrelevant on disk.
const std::size_t max_positions = 1000;

// A bounded, least-recently-used map of serialized positions.
//
// Not thread safe on its own - `check_logfile::bookmarks` wraps it in a lock.
// Kept separate from that wrapper so the eviction rules can be unit tested
// without a running module.
class store {
 public:
  typedef std::map<std::string, std::string> map_type;

  explicit store(std::size_t max_entries = max_positions) : max_entries_(max_entries == 0 ? 1 : max_entries), clock_(0) {}

  void put(const std::string &key, const std::string &value) {
    entry &e = entries_[key];
    e.value = value;
    e.used = ++clock_;
    trim();
  }

  // The stored value, or an empty string when the key is unknown. Counts as a
  // use: a bookmark which is checked but has nothing new to report must not
  // age out before one which is merely written to.
  std::string get(const std::string &key) {
    const impl_type::iterator it = entries_.find(key);
    if (it == entries_.end()) return "";
    it->second.used = ++clock_;
    return it->second.value;
  }

  map_type snapshot() const {
    map_type ret;
    for (const impl_type::value_type &v : entries_) {
      ret[v.first] = v.second.value;
    }
    return ret;
  }

  std::size_t size() const { return entries_.size(); }

 private:
  struct entry {
    std::string value;
    // Monotonic use counter; the lowest one is the next to go.
    std::uint64_t used;
    entry() : used(0) {}
  };
  typedef std::map<std::string, entry> impl_type;

  // Linear, but it only runs when the cap is exceeded and the cap is small.
  void trim() {
    while (entries_.size() > max_entries_) {
      impl_type::iterator oldest = entries_.begin();
      for (impl_type::iterator it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->second.used < oldest->second.used) oldest = it;
      }
      entries_.erase(oldest);
    }
  }

  std::size_t max_entries_;
  std::uint64_t clock_;
  impl_type entries_;
};

}  // namespace bookmark
}  // namespace check_logfile
