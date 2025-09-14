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

#include "filter.hpp"

#include <boost/assign.hpp>
#include <str/xtos.hpp>

#include "file_finder.hpp"

using namespace boost::assign;
using namespace parsers::where;

constexpr int file_type_file = 1;
constexpr int file_type_dir = 2;
constexpr int file_type_error = -1;
//////////////////////////////////////////////////////////////////////////

int convert_new_type(parsers::where::evaluation_context context, std::string str) {
  if (str == "critical") return 1;
  if (str == "error") return 2;
  if (str == "warning" || str == "warn") return 3;
  if (str == "informational" || str == "info" || str == "information" || str == "success" || str == "auditSuccess") return 4;
  if (str == "debug" || str == "verbose") return 5;
  try {
    return str::stox<int>(str);
  } catch (const std::exception &) {
    context->error("Failed to convert: " + str);
    return 2;
  }
}

parsers::where::node_type fun_convert_type(boost::shared_ptr<file_filter::filter_obj> object, parsers::where::evaluation_context context,
                                           parsers::where::node_type subject) {
  try {
    std::string key = subject->get_string_value(context);
    if (key == "file") return parsers::where::factory::create_int(file_type_file);
    if (key == "dir") return parsers::where::factory::create_int(file_type_dir);
    context->error("Failed to convert: " + key + " not file or dir");
    return parsers::where::factory::create_int(file_type_error);
  } catch (const std::exception &e) {
    context->error("Failed to convert type expression: " + utf8::utf8_from_native(e.what()));
  }
  return parsers::where::factory::create_int(-1);
}

file_filter::filter_obj_handler::filter_obj_handler() {
  const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

  registry_.add_string("path", &filter_obj::get_path, "Path of file")
      .add_string("version", &filter_obj::get_version, "Windows exe/dll file version")
      .add_string("filename", &filter_obj::get_filename, "The name of the file")
      .add_string("extension", &filter_obj::get_extension, "The filename extension")
      .add_string("file", &filter_obj::get_filename, "The name of the file")
      .add_string("name", &filter_obj::get_filename, "The name of the file")
      .add_string("access_l", &filter_obj::get_access_sl, "Last access time (local time)")
      .add_string("creation_l", &filter_obj::get_creation_sl, "When file was created (local time)")
      .add_string("written_l", &filter_obj::get_written_sl, "When file was last written  to (local time)")
      .add_string("access_u", &filter_obj::get_access_su, "Last access time (UTC)")
      .add_string("creation_u", &filter_obj::get_creation_su, "When file was created (UTC)")
      .add_string("written_u", &filter_obj::get_written_su, "When file was last written  to (UTC)");

  registry_.add_int_x("line_count", &filter_obj::get_line_count, "Number of lines in the file (text files)")
      .add_int_x("access", type_date, &filter_obj::get_access, "Last access time")
      .add_int_x("creation", type_date, &filter_obj::get_creation, "When file was created")
      .add_int_x("written", type_date, &filter_obj::get_write, "When file was last written to")
      .add_int_x("write", type_date, &filter_obj::get_write, "Alias for written")
      .add_int_x("age", type_int, &filter_obj::get_age, "Seconds since file was last written")
      .add_int_x("type", type_custom_type, &filter_obj::get_type, "Type of item (file or dir)");

  // clang-format off
  registry_.add_int()
    ("size", type_size, [] (auto obj, auto context) { return obj->get_size(); }, "File size").add_scaled_byte(std::string(""), " size")
    ("total", type_bool, [] (auto obj, auto context) { return obj->is_total(); }, "True if this is the total object").no_perf();
    ;

  registry_.add_converter()
    (type_custom_type, &fun_convert_type)
    ;
  // clang-format on

  registry_.add_human_string("access", &filter_obj::get_access_su, "")
      .add_human_string("creation", &filter_obj::get_creation_su, "")
      .add_human_string("written", &filter_obj::get_written_su, "")
      .add_human_string("type", &filter_obj::get_type_su, "");
}

//////////////////////////////////////////////////////////////////////////

#ifdef WIN32
boost::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::get(unsigned long long now, const WIN32_FIND_DATA info, boost::filesystem::path path) {
  return boost::shared_ptr<file_filter::filter_obj>(new file_filter::filter_obj(
      path, utf8::cvt<std::string>(info.cFileName), now,
      (info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime,
      (info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime,
      (info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime,
      (info.nFileSizeHigh * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.nFileSizeLow, info.dwFileAttributes));
};
#endif
boost::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::get_total(unsigned long long now) {
  return boost::shared_ptr<file_filter::filter_obj>(new file_filter::filter_obj("", "total", now, now, now, now, 0));
}

std::string file_filter::filter_obj::get_version() {
  if (cached_version) return *cached_version;
  std::string fullpath = (path / filename).string();

  DWORD dwDummy;
  DWORD dwFVISize = GetFileVersionInfoSize(utf8::cvt<std::wstring>(fullpath).c_str(), &dwDummy);
  if (dwFVISize == 0) return "";
  LPBYTE lpVersionInfo = new BYTE[dwFVISize + 1];
  if (!GetFileVersionInfo(utf8::cvt<std::wstring>(fullpath).c_str(), 0, dwFVISize, lpVersionInfo)) {
    delete[] lpVersionInfo;
    // handler->error("Failed to get version for " + fullpath + ": " + error::lookup::last_error());
    return "";
  }
  UINT uLen;
  VS_FIXEDFILEINFO *lpFfi;
  if (!VerQueryValue(lpVersionInfo, L"\\", (LPVOID *)&lpFfi, &uLen)) {
    delete[] lpVersionInfo;
    // handler->error("Failed to query version for " + fullpath + ": " + error::lookup::last_error());
    return "";
  }
  DWORD dwFileVersionMS = lpFfi->dwFileVersionMS;
  DWORD dwFileVersionLS = lpFfi->dwFileVersionLS;
  delete[] lpVersionInfo;
  DWORD dwLeftMost = HIWORD(dwFileVersionMS);
  DWORD dwSecondLeft = LOWORD(dwFileVersionMS);
  DWORD dwSecondRight = HIWORD(dwFileVersionLS);
  DWORD dwRightMost = LOWORD(dwFileVersionLS);
  cached_version.reset(str::xtos(dwLeftMost) + "." + str::xtos(dwSecondLeft) + "." + str::xtos(dwSecondRight) + "." + str::xtos(dwRightMost));
  return *cached_version;
}

unsigned long long file_filter::filter_obj::get_type() { return file_finder::is_directory(attributes) ? file_type_dir : file_type_file; }

std::string file_filter::filter_obj::get_type_su() { return file_finder::is_directory(attributes) ? "dir" : "file"; }

unsigned long file_filter::filter_obj::get_line_count() {
  if (cached_count) return *cached_count;

  unsigned long count = 0;
  std::string fullpath = (path / filename).string();
  FILE *pFile = fopen(fullpath.c_str(), "r");
  ;
  if (pFile == NULL) return 0;
  int c;
  do {
    c = fgetc(pFile);
    if (c == '\r') {
      c = fgetc(pFile);
      count++;
    } else if (c == '\n') {
      c = fgetc(pFile);
      count++;
    }
  } while (c != EOF);
  fclose(pFile);
  cached_count.reset(count);
  return *cached_count;
}

void file_filter::filter_obj::add(boost::shared_ptr<file_filter::filter_obj> info) { ullSize += info->ullSize; }

//////////////////////////////////////////////////////////////////////////