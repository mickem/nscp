// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <fts.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <str/utf8.hpp>
#include <string>
#include <vector>

#include "largest_files_scan.hpp"

// The POSIX data source for check_largest_files: a walk with fts(3), which
// glibc and the BSDs (macOS included) both provide.
//
// There is no filesystem-independent equivalent of the NTFS Master File
// Table to read here. What does exist is per filesystem and needs the raw
// block device (ext4's inode tables), root and a filesystem-specific ioctl
// (btrfs tree search, XFS bulkstat - which yields sizes but no names), so the
// walk is the portable answer. FTS_PHYSICAL is what keeps it off symbolic
// links: they come back as FTS_SL and are never followed. FTS_XDEV keeps it on
// the root's filesystem, which is also what an MFT scan does on Windows.

namespace largest_files {

scan_result scan(const scan_request &request) {
  scan_result result;
  result.method = "walk";

  if (request.method == scan_method::mft) {
    result.error = "method=mft is only available on Windows (NTFS)";
    return result;
  }

  struct stat st;
  if (lstat(request.root.c_str(), &st) != 0) {
    result.error = "Path not found: " + request.root;
    return result;
  }
  // The root is the one link that is followed: the caller named it.
  if (S_ISLNK(st.st_mode) && stat(request.root.c_str(), &st) != 0) {
    result.error = "Path not found: " + request.root;
    return result;
  }
  if (!S_ISDIR(st.st_mode)) {
    result.error = "Not a folder: " + request.root;
    return result;
  }

  top_n ranking(request.top, request.min_size);
  folder_rollup rollup(ranking, request.folder_depth);

  std::string root = request.root;
  while (root.size() > 1 && root.back() == '/') root.pop_back();
  std::vector<char> root_buffer(root.begin(), root.end());
  root_buffer.push_back('\0');
  char *paths[] = {root_buffer.data(), nullptr};

  int options = FTS_PHYSICAL | FTS_NOCHDIR | FTS_COMFOLLOW;
  if (request.same_filesystem) options |= FTS_XDEV;
  FTS *fts = fts_open(paths, options, nullptr);
  if (fts == nullptr) {
    result.error = "Unable to read folder " + request.root + ": " + utf8::utf8_from_native(std::strerror(errno));
    return result;
  }

  while (FTSENT *ent = fts_read(fts)) {
    // Every entry: the clock is cheap next to fts_read, which lists a whole
    // folder when it enters one.
    if (std::chrono::steady_clock::now() >= request.deadline) {
      result.timed_out = true;
      break;
    }
    const int level = static_cast<int>(ent->fts_level);
    switch (ent->fts_info) {
      case FTS_D:
        if (level > 0) result.folders_seen++;
        if (request.folders) rollup.enter(rollup.ranks(level) ? std::string(ent->fts_path) : std::string(), level);
        break;
      case FTS_DP:
        if (request.folders) rollup.leave();
        break;
      case FTS_F: {
        result.files_seen++;
        if (!request.pattern.empty() && !wildcard_match(request.pattern.c_str(), ent->fts_name, false)) break;
        const unsigned long long size = static_cast<unsigned long long>(ent->fts_statp->st_size);
        if (request.folders) {
          rollup.add_file(size);
        } else if (ranking.accepts(size)) {
          ranking.add(entry{ent->fts_path, size, 1});
        }
        break;
      }
      case FTS_SL:
      case FTS_SLNONE:
        result.links_skipped++;
        break;
      case FTS_DNR:
        // An unreadable folder comes back once, in place of both visits.
        result.unreadable++;
        if (level == 0) result.error = "Unable to read folder " + request.root + ": " + utf8::utf8_from_native(std::strerror(ent->fts_errno));
        break;
      case FTS_ERR:
      case FTS_NS:
        result.unreadable++;
        break;
      default:
        // FTS_DC (a cycle), FTS_DEFAULT (devices, sockets, fifos): no size
        // worth ranking.
        break;
    }
  }
  fts_close(fts);
  if (request.folders) rollup.finish();
  result.entries = ranking.take_sorted();
  return result;
}

}  // namespace largest_files
