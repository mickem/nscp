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

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <fstream>
#include <sstream>
#include <str/utf8.hpp>

#ifdef WIN32
// clang-format off
#include <win/windows.hpp>
#include <shellapi.h>
// clang-format on
#else
#include <boost/algorithm/string/case_conv.hpp>
#endif

namespace file_helpers {
namespace fs = boost::filesystem;

inline std::string read_file_as_string(const boost::filesystem::path& file) {
  const std::ifstream stream(file.c_str());
  if (!stream) {
    throw std::runtime_error("Failed to open file: " + file.string());
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

class checks {
 public:
  static bool is_directory(const std::string& path) { return fs::is_directory(path); }
  static bool is_file(const std::string& path) { return fs::is_regular_file(path); }
  static bool path_contains_file(fs::path dir, fs::path file) {
    dir = dir.lexically_normal();
    file = file.lexically_normal();
    if (dir.filename() == ".") dir.remove_filename();
    file.remove_filename();
    const std::size_t dir_len = std::distance(dir.begin(), dir.end());
    const std::size_t file_len = std::distance(file.begin(), file.end());
    if (dir_len > file_len) return false;
    return std::equal(dir.begin(), dir.end(), file.begin());
  }

  // Validate an entry name from an untrusted archive (zip, tar, ...) before
  // joining it onto a base directory and writing the contents. Returns true
  // and populates `out` with the normalised target on success. Returns false
  // (without touching `out`) if the entry is empty, contains a NUL, or would
  // resolve outside `base` after lexical normalisation. The check is
  // deliberately lexical: it does not consult the filesystem, so symlinked
  // intermediate directories are not followed - that's intentional, the
  // archive itself should not be trusted to declare its own destination.
  static bool is_safe_archive_entry(const fs::path& base, const std::string& entry_name, fs::path& out) {
    if (entry_name.empty()) return false;
    if (entry_name.find('\0') != std::string::npos) return false;
    const fs::path candidate = (base / entry_name).lexically_normal();
    if (!path_contains_file(base, candidate)) return false;
    out = candidate;
    return true;
  }
};

class meta {
 public:
  static std::string get_filename(const fs::path& path) { return path.filename().string(); }
  static std::string get_path(const std::string& file) {
    fs::path path(file);
    return path.parent_path().string();
  }
  static std::string get_filename(const std::string& file) { return get_filename(fs::path(file)); }
  static std::string get_extension(const fs::path& path) { return path.extension().string(); }
  static fs::path make_preferred(fs::path& path) { return path.make_preferred(); }
};

class patterns {
 public:
  typedef std::pair<fs::path, fs::path> pattern_type;

  static pattern_type split_pattern(const fs::path& path) {
    if (fs::is_directory(path)) return pattern_type(path, fs::path());
    return pattern_type(path.parent_path(), path.filename());
  }
  static pattern_type split_path_ex(const fs::path& path) {
    if (fs::is_directory(path)) {
      return pattern_type(path, fs::path());
    }

    std::string spath = path.string();
    std::string::size_type pos = spath.find_last_of('\\');
    if (pos == std::string::npos) {
      pattern_type(spath, fs::path("*.*"));
    }
    return pattern_type(spath.substr(0, pos), spath.substr(pos + 1));
  }
  static fs::path combine_pattern(const pattern_type& pattern) { return pattern.first / pattern.second; }

  static std::string glob_to_regexp(std::string mask) {
    boost::algorithm::replace_all(mask, ".", "\\.");
    boost::algorithm::replace_all(mask, "*", ".*");
    boost::algorithm::replace_all(mask, "?", ".");
    return mask;
  }
};  // END patterns

struct finder {
  static boost::optional<boost::filesystem::path> locate_file_icase(const boost::filesystem::path& path, const std::string& filename) {
    if (!exists(path)) {
      return boost::optional<boost::filesystem::path>();
    }
    boost::filesystem::path fullpath = path / filename;
#ifdef WIN32
    auto tmp = utf8::cvt<std::wstring>(fullpath.string());
    SHFILEINFOW sfi = {nullptr};
    boost::replace_all(tmp, "/", "\\");
    const auto hr = SHGetFileInfo(tmp.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME);
    if (hr != 0) {
      tmp = sfi.szDisplayName;
      boost::filesystem::path rpath = path / utf8::cvt<std::string>(tmp);
      if (boost::filesystem::is_regular_file(rpath)) return rpath;
    }
#else
    if (boost::filesystem::is_regular_file(fullpath)) {
      return fullpath;
    }
    boost::filesystem::directory_iterator eod;
    std::string tmp = boost::algorithm::to_lower_copy(filename);
    for (boost::filesystem::directory_iterator it(path); it != eod; ++it) {
      if (boost::filesystem::is_regular_file(it->path()) && boost::algorithm::to_lower_copy(file_helpers::meta::get_filename(it->path())) == tmp) {
        return it->path();
      }
    }
#endif
    return boost::optional<boost::filesystem::path>();
  }
};
}  // namespace file_helpers
