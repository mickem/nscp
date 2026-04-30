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

#include "file_finder.hpp"

#include <file_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <str/utf8.hpp>

#include "filter.hpp"

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD) - 1)
#endif
#ifndef FILE_ATTRIBUTE_DIRECTORY
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#endif
#ifndef FILE_ATTRIBUTE_REPARSE_POINT
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#endif
bool file_finder::is_directory(unsigned long dwAttr) {
  if (dwAttr == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
    return true;
  }
  return false;
}

void file_finder::recursive_scan(file_filter::filter &filter, scanner_context &context, const boost::filesystem::path &dir,
                                 const boost::shared_ptr<file_filter::filter_obj> &total_obj, const bool total_all, const bool recursive,
                                 const int current_level) {
  if (!context.is_valid_level(current_level)) {
    if (context.debug) context.report_debug("Level death exhausted: " + str::xtos(current_level));
    return;
  }
  WIN32_FIND_DATA wfd;

  // Always convert via utf8::cvt so that paths containing non-ACP characters
  // (e.g. accented letters) are passed correctly to the Win32 wide APIs.
  // Using boost::filesystem::path::wstring() honours the path's imbued locale
  // (system codepage by default on Windows) and silently mangles non-ACP
  // characters, which caused issue #598 ("File was NOT found! Invalid file
  // specified" for paths like D:/DepotInterface/Reintégrer/).
  const std::wstring wide_dir = utf8::cvt<std::wstring>(dir.string());

  const DWORD fileAttr = GetFileAttributes(wide_dir.c_str());
  if ((fileAttr == INVALID_FILE_ATTRIBUTES) && (!recursive)) {
    // Top-level path supplied by the user does not exist (or is not
    // accessible). Record it so the caller can surface this as an UNKNOWN
    // result instead of silently returning "No files found" (issue #613).
    context.missing_paths.push_back(dir.string());
    context.report_error("Invalid file specified: " + dir.string());
    return;
  } else if (fileAttr == INVALID_FILE_ATTRIBUTES) {
    context.report_warning("Invalid file specified: " + dir.string());
    return;
  }

  if (!is_directory(fileAttr)) {
    if (context.debug) context.report_debug("Found a file won't do recursive scan: " + dir.string());
    // It is a file check it and return (don't check recursively)
    file_helpers::patterns::pattern_type single_path = file_helpers::patterns::split_path_ex(dir.string());
    if (context.debug) context.report_debug("Path is: " + single_path.first.string());
    HANDLE hFind = FindFirstFile(wide_dir.c_str(), &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
      boost::shared_ptr<file_filter::filter_obj> info = file_filter::filter_obj::get(context.now, wfd, single_path.first);
      // boost::make_shared<eventlog_filter::filter_obj>(record, filter.summary.count_match)
      modern_filter::match_result ret = filter.match(info);
      if (total_obj && (ret.matched_filter || total_all)) total_obj->add(info);
      FindClose(hFind);
    } else {
      context.report_error("File was NOT found!");
    }
    return;
  }
  std::string file_pattern = dir.string() + "\\" + context.pattern;
  HANDLE hFind = FindFirstFile(utf8::cvt<std::wstring>(file_pattern).c_str(), &wfd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (is_directory(wfd.dwFileAttributes) && (wcscmp(wfd.cFileName, L".") == 0 || wcscmp(wfd.cFileName, L"..") == 0)) continue;
      boost::shared_ptr<file_filter::filter_obj> info = file_filter::filter_obj::get(context.now, wfd, dir);
      modern_filter::match_result ret = filter.match(info);
      if (total_obj && (ret.matched_filter || total_all)) total_obj->add(info);
    } while (FindNextFile(hFind, &wfd));
    FindClose(hFind);
  }
  std::string dir_pattern = dir.string() + "\\*.*";
  // if (context.debug) context.report_debug("File pattern: " + dir_pattern);
  hFind = FindFirstFile(utf8::cvt<std::wstring>(dir_pattern).c_str(), &wfd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (is_directory(wfd.dwFileAttributes)) {
        // Skip reparse points (NTFS junctions, symlinks, mount points). They
        // can point back into the same volume / directory tree and caused
        // check_files to enumerate (and count) the same files multiple times,
        // e.g. inside C:\Program Files (x86)\Trend Micro\... (issue #605).
        if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
          if (context.debug) context.report_debug(std::string("Skipping reparse point: ") + utf8::cvt<std::string>(wfd.cFileName));
          continue;
        }
        if ((wcscmp(wfd.cFileName, L".") != 0) && (wcscmp(wfd.cFileName, L"..") != 0))
          recursive_scan(filter, context, dir / wfd.cFileName, total_obj, total_all, true, current_level + 1);
      }
    } while (FindNextFile(hFind, &wfd));
    FindClose(hFind);
  }
}

bool file_finder::scanner_context::is_valid_level(int current_level) const {
  // max_depth == -1 means unlimited recursion.
  // max_depth == 0 means "scan the top directory only" (no recursion).
  // max_depth == N (N >= 1) means "recurse up to N levels below the top directory".
  if (max_depth == -1) return true;
  if (max_depth == 0) return current_level == 0;
  return current_level < max_depth;
}

void file_finder::scanner_context::report_error(const std::string &str) const { NSC_LOG_ERROR(str); }

void file_finder::scanner_context::report_debug(const std::string &str) const {
  if (debug) NSC_DEBUG_MSG(str);
}

void file_finder::scanner_context::report_warning(const std::string &msg) const { NSC_LOG_ERROR(msg); }

boost::shared_ptr<file_filter::filter_obj> file_finder::stat_single_file(const boost::filesystem::path &path, long long now) {
  WIN32_FIND_DATA wfd;
  const std::wstring wide_path = utf8::cvt<std::wstring>(path.string());
  HANDLE hFind = FindFirstFile(wide_path.c_str(), &wfd);
  if (hFind == INVALID_HANDLE_VALUE) {
    return boost::shared_ptr<file_filter::filter_obj>();
  }
  // FindFirstFile populates wfd with the entry for "path" itself; the parent
  // directory is what filter_obj::get expects in its third argument so the
  // rendered "path" / "filename" columns work the same as for check_files.
  file_helpers::patterns::pattern_type single_path = file_helpers::patterns::split_path_ex(path.string());
  boost::shared_ptr<file_filter::filter_obj> info = file_filter::filter_obj::get(now, wfd, single_path.first);
  FindClose(hFind);
  return info;
}