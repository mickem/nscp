// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unix implementation of the file scanner used by check_files / check_single_file.
// Mirrors file_finder.cpp (Windows) but walks the tree with
// boost::filesystem + stat and matches names with POSIX fnmatch.

#include "file_finder.hpp"

#include <fnmatch.h>
#include <sys/stat.h>

#include <boost/filesystem.hpp>
#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <str/xtos.hpp>

#include "filter.hpp"

namespace {
// Honour the historical "*.*" / "*" = "match everything" rule on top of fnmatch.
bool pattern_matches(const std::string &pattern, const std::string &name) {
  if (pattern.empty() || pattern == "*" || pattern == "*.*") return true;
  return fnmatch(pattern.c_str(), name.c_str(), 0) == 0;
}

std::shared_ptr<file_filter::filter_obj> make_obj(const boost::filesystem::path &dir, const std::string &name, const struct stat &st, long long now) {
  return file_filter::filter_obj::create(dir, name, static_cast<unsigned long long>(st.st_size), static_cast<long long>(st.st_ctime),
                                         static_cast<long long>(st.st_atime), static_cast<long long>(st.st_mtime), S_ISDIR(st.st_mode), now);
}
}  // namespace

// Windows attribute helper; unused on Unix but part of the shared interface.
bool file_finder::is_directory(unsigned long) { return false; }

void file_finder::recursive_scan(file_filter::filter &filter, scanner_context &context, const boost::filesystem::path &dir,
                                 const std::shared_ptr<file_filter::filter_obj> &total_obj, const bool total_all, const bool recursive,
                                 const int current_level) {
  if (!context.is_valid_level(current_level)) {
    if (context.debug) context.report_debug("Level depth exhausted: " + str::xtos(current_level));
    return;
  }

  struct stat st;
  if (::stat(dir.string().c_str(), &st) != 0) {
    if (!recursive) {
      // Top-level path supplied by the user does not exist / is inaccessible.
      // Record it so the caller surfaces UNKNOWN rather than "No files found".
      context.missing_paths.push_back(dir.string());
      context.report_error("Invalid file specified: " + dir.string());
    } else {
      context.report_warning("Invalid file specified: " + dir.string());
    }
    return;
  }

  if (!S_ISDIR(st.st_mode)) {
    // A single file: check it and return (no recursion).
    std::shared_ptr<file_filter::filter_obj> info = make_obj(dir.parent_path(), dir.filename().string(), st, context.now);
    const modern_filter::match_result ret = filter.match(info);
    if (total_obj && (ret.matched_filter || total_all)) total_obj->add(info);
    return;
  }

  boost::system::error_code ec;
  boost::filesystem::directory_iterator it(dir, ec), end;
  if (ec) {
    context.report_warning("Failed to open directory: " + dir.string());
    return;
  }
  for (; it != end; it.increment(ec)) {
    if (ec) break;
    const boost::filesystem::path &p = it->path();
    struct stat est;
    // lstat so symlinks are detected (and skipped) rather than followed — this
    // mirrors the Windows reparse-point skip that prevents double counting and
    // recursion loops.
    if (::lstat(p.string().c_str(), &est) != 0) continue;
    const std::string name = p.filename().string();
    if (S_ISLNK(est.st_mode)) {
      if (context.debug) context.report_debug("Skipping symlink: " + name);
      continue;
    }
    if (S_ISDIR(est.st_mode)) {
      recursive_scan(filter, context, p, total_obj, total_all, true, current_level + 1);
      continue;
    }
    if (!pattern_matches(context.pattern, name)) continue;
    std::shared_ptr<file_filter::filter_obj> info = make_obj(dir, name, est, context.now);
    const modern_filter::match_result ret = filter.match(info);
    if (total_obj && (ret.matched_filter || total_all)) total_obj->add(info);
  }
}

bool file_finder::scanner_context::is_valid_level(int current_level) const {
  if (max_depth == -1) return true;
  if (max_depth == 0) return current_level == 0;
  return current_level < max_depth;
}

void file_finder::scanner_context::report_error(const std::string &str) { NSC_LOG_ERROR(str); }
void file_finder::scanner_context::report_debug(const std::string &str) const {
  if (debug) NSC_DEBUG_MSG(str);
}
void file_finder::scanner_context::report_warning(const std::string &msg) { NSC_LOG_ERROR(msg); }

std::shared_ptr<file_filter::filter_obj> file_finder::stat_single_file(const boost::filesystem::path &path, long long now) {
  struct stat st;
  if (::stat(path.string().c_str(), &st) != 0) return {};
  // The single-file API is only defined for regular files.
  if (S_ISDIR(st.st_mode)) return {};
  return make_obj(path.parent_path(), path.filename().string(), st, now);
}
