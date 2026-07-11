// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_share.hpp"

#include <boost/algorithm/string.hpp>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#ifdef WIN32
#include <win/wmi/wmi_query.hpp>
#endif

namespace po = boost::program_options;

namespace share_check {

#ifdef WIN32
shares_type query() {
  shares_type shares;
  try {
    wmi_impl::query q("select Name, Path, Description, Type from Win32_Share", "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      share_info s;
      s.name = r.get_string("Name");
      s.path = r.get_string("Path");
      s.description = r.get_string("Description");
      s.type_raw = r.get_int("Type");
      s.exists = true;
      shares.push_back(s);
    }
  } catch (const wmi_impl::wmi_exception &) {
    // Win32_Share unavailable: report no shares rather than failing.
  }
  return shares;
}
#else
shares_type query() { return {}; }
#endif

namespace check {

typedef share_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  using parsers::where::type_bool;
  registry_.add_string_var("name", &filter_obj::get_name, "Share name (e.g. C$, Public)")
      .add_string_var("path", &filter_obj::get_path, "Local path the share maps to (empty for IPC$)")
      .add_string_var("description", &filter_obj::get_description, "Share description / comment")
      .add_string_var("type", &filter_obj::get_type, "Share kind: disk, printer, device, ipc or unknown");
  registry_.add_int_var("is_admin", type_bool, &filter_obj::get_is_admin, "1 for an administrative share (C$, ADMIN$, IPC$)")
      .add_int_var("exists", type_bool, &filter_obj::get_exists, "1 if the share exists (0 for a requested-but-missing share)");
}

void check_share(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  std::vector<std::string> required;

  filter_type filter;
  // Default: CRITICAL when a share is missing. When no share= is given all rows
  // exist, so this is inert and the check just lists shares (OK). empty_state=ok:
  // a host with no shares is not inherently a problem.
  filter_helper.add_options("", "not exists", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${name} (type=${type}, path=${path}, exists=${exists})", "${name}", "%(status): No shares found",
                           "%(status): All %(count) share(s) ok.");
  filter_helper.set_default_perf_config("extra(count)");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("share", po::value<std::vector<std::string>>(&required),
     "Require a specific share to exist (repeatable). The check is CRITICAL when a requested share is missing. "
     "When omitted, all shares are listed instead.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  const shares_type all = query();

  if (required.empty()) {
    // List mode: one row per existing share.
    for (const share_info &s : all) {
      std::shared_ptr<filter_obj> record(new filter_obj(s));
      filter.match(record);
    }
  } else {
    // Required mode: one row per requested share, present or missing. Share
    // names are case-insensitive on Windows.
    for (const std::string &req : required) {
      share_info row;
      bool found = false;
      for (const share_info &s : all) {
        if (boost::iequals(s.name, req)) {
          row = s;
          found = true;
          break;
        }
      }
      if (!found) {
        row = share_info();
        row.name = req;
        row.exists = false;
      }
      std::shared_ptr<filter_obj> record(new filter_obj(row));
      filter.match(record);
    }
  }

  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace share_check
