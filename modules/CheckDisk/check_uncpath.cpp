// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_uncpath.hpp"

#include <boost/program_options.hpp>
#include <error/error.hpp>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <vector>

#ifdef WIN32
#include <Windows.h>
#include <winnetwk.h>
#pragma comment(lib, "mpr.lib")
#else
#include <sys/statvfs.h>
#endif

namespace po = boost::program_options;

namespace uncpath_check {

std::string unc_obj::get_free_human() const { return str::format::format_byte_units(free); }
std::string unc_obj::get_used_human() const { return str::format::format_byte_units(get_used()); }
std::string unc_obj::get_size_human() const { return str::format::format_byte_units(size); }

#ifdef WIN32
unc_obj query(const std::string &path, const std::string &user, const std::string &password) {
  unc_obj o;
  o.path = path;

  const std::wstring wpath = utf8::cvt<std::wstring>(path);
  bool connected = false;
  if (!user.empty()) {
    NETRESOURCEW nr = {0};
    nr.dwType = RESOURCETYPE_DISK;
    nr.lpRemoteName = const_cast<LPWSTR>(wpath.c_str());
    const std::wstring wuser = utf8::cvt<std::wstring>(user);
    const std::wstring wpass = utf8::cvt<std::wstring>(password);
    const DWORD rc = WNetAddConnection2W(&nr, wpass.empty() ? nullptr : const_cast<LPWSTR>(wpass.c_str()),
                                         wuser.empty() ? nullptr : const_cast<LPWSTR>(wuser.c_str()), 0);
    if (rc != NO_ERROR) {
      o.error = "Failed to connect to " + path + " as " + user + ": " + error::lookup::last_error(rc);
      return o;
    }
    connected = true;
  }

  ULARGE_INTEGER free_avail, total_bytes, total_free;
  const BOOL ok = GetDiskFreeSpaceExW(wpath.c_str(), &free_avail, &total_bytes, &total_free);
  const DWORD err = ok ? NO_ERROR : GetLastError();
  if (ok) {
    o.size = static_cast<long long>(total_bytes.QuadPart);
    o.free = static_cast<long long>(total_free.QuadPart);
    o.user_free = static_cast<long long>(free_avail.QuadPart);
    o.ok = true;
  } else {
    o.error = "Failed to query " + path + ": " + error::lookup::last_error(err);
  }

  if (connected) WNetCancelConnection2W(wpath.c_str(), 0, TRUE);
  return o;
}
#else
unc_obj query(const std::string &path, const std::string & /*user*/, const std::string & /*password*/) {
  unc_obj o;
  o.path = path;
  // Alternate-credential UNC access is a Windows concept; on Unix we can only
  // report free space for an already-mounted path via statvfs.
  struct statvfs st {};
  if (statvfs(path.c_str(), &st) != 0) {
    o.error = "Failed to statvfs " + path + " (UNC credential access is Windows-only)";
    return o;
  }
  const long long bs = static_cast<long long>(st.f_frsize);
  o.size = static_cast<long long>(st.f_blocks) * bs;
  o.free = static_cast<long long>(st.f_bfree) * bs;
  o.user_free = static_cast<long long>(st.f_bavail) * bs;
  o.ok = true;
  return o;
}
#endif

namespace check {

typedef unc_obj filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("path", &filter_obj::get_path, "The UNC path being checked");

  // Distinct perf suffixes so co-referenced metrics each get their own series
  // ('<path>_free', ...) instead of collapsing onto the shared ${path} alias.
  registry_.add_int_var("size", &filter_obj::get_size, "Total size of the share in bytes")
      .add_int_perf("B", "", "_size")
      .add_int_var("free", &filter_obj::get_free, "Free space on the share in bytes")
      .add_int_perf("B", "", "_free")
      .add_int_var("used", &filter_obj::get_used, "Used space on the share in bytes")
      .add_int_perf("B", "", "_used")
      .add_int_var("user_free", &filter_obj::get_user_free, "Free space available to the querying user (quota-aware) in bytes")
      .add_int_perf("B", "", "_user_free")
      .add_int_var("free_pct", &filter_obj::get_free_pct, "Percentage of free space")
      .add_int_perf("%", "", "_free_pct")
      .add_int_var("used_pct", &filter_obj::get_used_pct, "Percentage of used space")
      .add_int_perf("%", "", "_used_pct");

  registry_.add_human_string("size", &filter_obj::get_size_human, "")
      .add_human_string("free", &filter_obj::get_free_human, "")
      .add_human_string("used", &filter_obj::get_used_human, "");
}

void check_uncpath(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);
  std::vector<std::string> paths;
  std::string user, password;

  filter_type filter;
  // Mirror check_drivesize defaults.
  filter_helper.add_options("used_pct > 80", "used_pct > 90", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${path}: ${used}/${size} used (${free} free)", "${path}", "%(status): No paths checked",
                           "%(status): All UNC paths are ok.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("path", po::value<std::vector<std::string>>(&paths), "The UNC path(s) to check, e.g. \\\\server\\share. Repeat for multiple.")
    ("user", po::value<std::string>(&user), "Optional user name for alternate credentials (Windows).")
    ("password", po::value<std::string>(&password), "Optional password for alternate credentials (Windows).")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (paths.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No path specified (use path=\\\\server\\share)");
  if (!filter_helper.build_filter(filter)) return;

  for (const std::string &p : paths) {
    unc_obj o = query(p, user, password);
    if (!o.ok) return nscapi::protobuf::functions::set_response_bad(*response, o.error);
    std::shared_ptr<filter_obj> record(new filter_obj(o));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace uncpath_check
