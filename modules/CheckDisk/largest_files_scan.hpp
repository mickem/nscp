// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <chrono>
#include <cstddef>
#include <cwctype>
#include <string>
#include <vector>

// The data side of check_largest_files: what a scan is asked for, what it
// hands back, and the two collectors every source shares. The sources
// themselves are per-platform (largest_files_source_win.cpp reads the NTFS
// Master File Table or walks the tree, largest_files_source_unix.cpp walks
// with fts(3)); everything in this header is platform neutral so the ranking
// and the folder roll-up are unit tested once for both.
namespace largest_files {

enum class scan_method { automatic, mft, walk };

struct scan_request {
  // The folder to scan, already held against [/settings/disk] 'file access'.
  std::string root;
  // Rank folders (the recursive total of everything beneath each one) rather
  // than files.
  bool folders = false;
  // Which folders are ranked: 1 = the root's direct children, N = everything
  // down to N levels below the root, -1 = every folder at any depth. Totals
  // are always recursive whatever this is set to.
  int folder_depth = 1;
  // Files (or, in folder mode, folder totals) below this are not ranked.
  unsigned long long min_size = 0;
  // How many entries to return.
  std::size_t top = 10;
  // A file name mask (* and ?). Empty matches everything. In folder mode it
  // selects which files count towards the totals.
  std::string pattern;
  // Stay on the root's filesystem (POSIX; an MFT scan never leaves its volume
  // and the Windows walker never enters a mount point, which is a reparse
  // point).
  bool same_filesystem = true;
  scan_method method = scan_method::automatic;
  std::chrono::steady_clock::time_point deadline = (std::chrono::steady_clock::time_point::max)();
};

struct entry {
  std::string path;
  unsigned long long size = 0;
  // Files counted in a folder's total; 1 for a file.
  unsigned long long files = 0;
};

struct scan_result {
  // Largest first, at most `top` of them.
  std::vector<entry> entries;
  // Set when the deadline passed before the scan finished: `entries` is then
  // the best of what was seen.
  bool timed_out = false;
  // "mft" or "walk".
  std::string method;
  unsigned long long files_seen = 0;
  unsigned long long folders_seen = 0;
  // Symbolic links, junctions and mount points that were not followed.
  unsigned long long links_skipped = 0;
  // Folders that could not be opened.
  unsigned long long unreadable = 0;
  // Why method=auto walked instead of reading the MFT (Windows), for the
  // timeout message: the failed attempt has spent part of the time.
  std::string fallback_reason;
  // Set when the scan could not run at all (missing root, MFT forced but not
  // available, ...). The check reports it as UNKNOWN.
  std::string error;
};

// Run a scan. Implemented once per platform.
scan_result scan(const scan_request &request);

// A file-name mask match: * matches any run, ? any single character. Neither
// treats a separator specially, since only a bare name is ever matched.
template <class C>
bool wildcard_match(const C *pattern, const C *name, bool ignore_case) {
  const C *star = nullptr;
  const C *resume = nullptr;
  auto same = [ignore_case](C a, C b) {
    if (a == b) return true;
    if (!ignore_case) return false;
    return std::towlower(static_cast<wint_t>(a)) == std::towlower(static_cast<wint_t>(b));
  };
  while (*name) {
    if (*pattern == C('*')) {
      star = pattern++;
      resume = name;
    } else if (*pattern == C('?') || (*pattern && same(*pattern, *name))) {
      ++pattern;
      ++name;
    } else if (star) {
      pattern = star + 1;
      name = ++resume;
    } else {
      return false;
    }
  }
  while (*pattern == C('*')) ++pattern;
  return *pattern == C(0);
}

// The bounded "largest N" selection every source feeds. `accepts()` is cheap
// and is asked first, so a source only builds the path string for a
// candidate that would actually make the list.
class top_n {
 public:
  top_n(std::size_t n, unsigned long long min_size) : n_(n), min_size_(min_size) {}

  // True for a size that could make the list (a tie may still lose to the
  // path order in add()).
  bool accepts(unsigned long long size) const {
    if (n_ == 0 || size < min_size_) return false;
    return heap_.size() < n_ || size >= heap_.front().size;
  }
  void add(entry e);
  // Largest first; ties broken on the path so the order is stable.
  std::vector<entry> take_sorted();

 private:
  std::size_t n_;
  unsigned long long min_size_;
  std::vector<entry> heap_;  // a min-heap on size
};

// Rolls file sizes up into folder totals for a source which walks the tree
// depth first. The source calls enter() for each folder before its contents
// and leave() after them; the root is entered at level 0. A folder is offered
// to the ranking when it is left, which is when its total is final.
class folder_rollup {
 public:
  folder_rollup(top_n &ranking, int folder_depth) : ranking_(ranking), folder_depth_(folder_depth) {}

  // `level` is the folder's depth below the root. Any frame at the same or a
  // deeper level is left first, so a source which loses a post-order visit
  // (an unreadable folder) cannot misattribute the sizes that follow.
  void enter(const std::string &path, int level);
  void add_file(unsigned long long size);
  void leave();
  // Leave every folder still open: the end of the walk, or a timeout.
  void finish();

  bool ranks(int level) const { return level >= 1 && (folder_depth_ < 0 || level <= folder_depth_); }

 private:
  struct frame {
    std::string path;
    int level;
    unsigned long long size;
    unsigned long long files;
  };
  top_n &ranking_;
  int folder_depth_;
  std::vector<frame> stack_;
};

}  // namespace largest_files
