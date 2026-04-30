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

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>

#ifdef WIN32
#include <Windows.h>
#endif

namespace file_filter {
struct file_object_exception : std::exception {
  std::string error_;

  explicit file_object_exception(const std::string& error) : error_(error) {}
  ~file_object_exception() throw() override {}
  const char* what() const throw() override { return error_.c_str(); }
};

struct filter_obj {
  filter_obj() : ullSize(0), ullCreationTime(0), ullLastAccessTime(0), ullLastWriteTime(0), ullNow(0), is_total_(false), attributes(0) {}
  filter_obj(const boost::filesystem::path& path_, const std::string& filename_, const __int64 now = 0, const __int64 creationTime = 0,
             const __int64 lastAccessTime = 0, const __int64 lastWriteTime = 0, const __int64 size = 0, const DWORD attributes = 0)
      : ullSize(size),
        ullCreationTime(creationTime),
        ullLastAccessTime(lastAccessTime),
        ullLastWriteTime(lastWriteTime),
        ullNow(now),
        filename(filename_),
        is_total_(false),
        path(path_),
        attributes(attributes) {}

  filter_obj(const filter_obj& other)
      : ullSize(other.ullSize),
        ullCreationTime(other.ullCreationTime),
        ullLastAccessTime(other.ullLastAccessTime),
        ullLastWriteTime(other.ullLastWriteTime),
        ullNow(other.ullNow),
        filename(other.filename),
        is_total_(false),
        path(other.path),
        cached_version(other.cached_version),
        cached_count(other.cached_count),
        attributes(other.attributes) {}

  const filter_obj& operator=(const filter_obj& other) {
    ullSize = other.ullSize;
    ullCreationTime = other.ullCreationTime;
    ullLastAccessTime = other.ullLastAccessTime;
    ullLastWriteTime = other.ullLastWriteTime;
    ullNow = other.ullNow;
    filename = other.filename;
    path = other.path;
    cached_version = other.cached_version;
    cached_count = other.cached_count;
    attributes = other.attributes;
    return *this;
  }

#ifdef WIN32
  static boost::shared_ptr<filter_obj> get(unsigned long long now, const WIN32_FIND_DATA& info, boost::filesystem::path path);
#endif
  static boost::shared_ptr<filter_obj> get_total(unsigned long long now);
  std::string get_filename() { return filename; }
  std::string get_path() const { return path.string(); }
  std::string show() const { return path.string() + "\\" + filename; }

  long long get_creation() const { return str::format::filetime_to_time(ullCreationTime); }
  long long get_access() const { return str::format::filetime_to_time(ullLastAccessTime); }
  long long get_write() const { return str::format::filetime_to_time(ullLastWriteTime); }
  long long get_age() const {
    const long long now = parsers::where::constants::get_now();
    return now - get_write();
  }
  static __int64 to_local_time(const __int64& t) {
    FILETIME ft;
    ft.dwHighDateTime = t >> 32;
    ft.dwLowDateTime = static_cast<DWORD>(t & 0xFFFFFFFF);
    const FILETIME lft = ft_utc_to_local_time(ft);
    return (lft.dwHighDateTime * (static_cast<unsigned long long>(MAXDWORD) + 1)) + static_cast<unsigned long long>(lft.dwLowDateTime);
  }

  static FILETIME ft_utc_to_local_time(const FILETIME& ft) {
    FILETIME lft;
    SYSTEMTIME st1, st2;
    FileTimeToSystemTime(&ft, &st1);
    SystemTimeToTzSpecificLocalTime(nullptr, &st1, &st2);
    SystemTimeToFileTime(&st2, &lft);
    return lft;
  }

  std::string get_creation_su() const { return str::format::format_filetime(ullCreationTime); }
  std::string get_access_su() const { return str::format::format_filetime(ullLastAccessTime); }
  std::string get_written_su() const { return str::format::format_filetime(ullLastWriteTime); }
  std::string get_creation_sl() const { return str::format::format_filetime(to_local_time(ullCreationTime)); }
  std::string get_access_sl() const { return str::format::format_filetime(to_local_time(ullLastAccessTime)); }
  std::string get_written_sl() const { return str::format::format_filetime(to_local_time(ullLastWriteTime)); }
  std::string get_extension() const {
    const auto extension_with_dot = boost::filesystem::path(filename).extension().string();
    return (extension_with_dot.size() > 1 && extension_with_dot[0] == '.') ? extension_with_dot.substr(1) : "";
  }
  unsigned long long get_type() const;
  std::string get_type_su() const;

  unsigned long long get_size() const { return ullSize; }
  std::string get_version(parsers::where::evaluation_context context);
  unsigned long get_line_count();

  void add(const boost::shared_ptr<filter_obj>& info);
  void make_total() { is_total_ = true; }
  bool is_total() const { return is_total_; }

  unsigned long long ullSize;
  __int64 ullCreationTime;
  __int64 ullLastAccessTime;
  __int64 ullLastWriteTime;
  __int64 ullNow;
  std::string filename;
  bool is_total_;
  boost::filesystem::path path;
  boost::optional<std::string> cached_version;
  boost::optional<unsigned long> cached_count;
  DWORD attributes;
};

typedef boost::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace file_filter