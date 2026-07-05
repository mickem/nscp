// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <memory>
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

// A file/dir with timestamps stored as native unix time (seconds since the
// epoch). The Windows builder (filter_win.cpp) converts FILETIME to unix time
// so the where-engine, derived fields and field registration are identical on
// both platforms; only the platform builders and the (Windows-only) version
// lookup differ.
struct filter_obj {
  filter_obj() : ullSize(0), creation_time(0), access_time(0), write_time(0), now(0), is_total_(false), is_dir_(false) {}

  std::string get_filename() const { return filename; }
  std::string get_path() const { return path.string(); }
  std::string show() const { return (path / filename).string(); }

  long long get_creation() const { return creation_time; }
  long long get_access() const { return access_time; }
  long long get_write() const { return write_time; }
  long long get_age() const { return parsers::where::constants::get_now() - get_write(); }

  std::string get_creation_su() const { return str::format::format_date(static_cast<std::time_t>(creation_time)); }
  std::string get_access_su() const { return str::format::format_date(static_cast<std::time_t>(access_time)); }
  std::string get_written_su() const { return str::format::format_date(static_cast<std::time_t>(write_time)); }
  std::string get_creation_sl() const { return format_local(creation_time); }
  std::string get_access_sl() const { return format_local(access_time); }
  std::string get_written_sl() const { return format_local(write_time); }
  static std::string format_local(long long t);

  std::string get_extension() const {
    const auto ext = boost::filesystem::path(filename).extension().string();
    return (ext.size() > 1 && ext[0] == '.') ? ext.substr(1) : "";
  }
  unsigned long long get_type() const;
  std::string get_type_su() const;

  unsigned long long get_size() const { return ullSize; }
  // Windows PE/DLL file version. Always "" on Unix (no such concept).
  std::string get_version(parsers::where::evaluation_context context);
  unsigned long get_line_count();

  // Lazily-computed file content checksums (lower-case hex). Return "" when the
  // file cannot be read or the build has no OpenSSL. Computed only when the
  // corresponding keyword is referenced, and cached per object.
  std::string get_md5();
  std::string get_sha1();
  std::string get_sha256();
  std::string get_sha384();
  std::string get_sha512();

  void add(const std::shared_ptr<filter_obj>& info) { ullSize += info->ullSize; }
  void make_total() { is_total_ = true; }
  bool is_total() const { return is_total_; }

#ifdef WIN32
  static std::shared_ptr<filter_obj> get(unsigned long long now, const WIN32_FIND_DATA& info, boost::filesystem::path path);
#endif
  // Neutral factory shared by both platform builders.
  static std::shared_ptr<filter_obj> create(const boost::filesystem::path& dir, const std::string& name, unsigned long long size, long long creation_time,
                                            long long access_time, long long write_time, bool is_dir, long long now);
  static std::shared_ptr<filter_obj> get_total(unsigned long long now);

  unsigned long long ullSize;
  long long creation_time;  // unix epoch seconds
  long long access_time;
  long long write_time;
  long long now;
  std::string filename;
  bool is_total_;
  bool is_dir_;
  boost::filesystem::path path;
  boost::optional<std::string> cached_version;
  boost::optional<unsigned long> cached_count;
  boost::optional<std::string> cached_md5;
  boost::optional<std::string> cached_sha1;
  boost::optional<std::string> cached_sha256;
  boost::optional<std::string> cached_sha384;
  boost::optional<std::string> cached_sha512;
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace file_filter
