/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include "check_registry.hpp"

#include <parsers/filter/cli_helper.hpp>
#include <nsclient/nsclient_exception.hpp>

namespace po = boost::program_options;

// ══════════════════════════════════════════════════════════════════════════════
//  check_registry_key
// ══════════════════════════════════════════════════════════════════════════════

namespace registry_key_checks {
namespace check_rk_filter {

using namespace parsers::where;

// Converter: allow filter expressions like  type = 'REG_SZ'
node_type parse_type_converter(boost::shared_ptr<filter_obj> /*obj*/, evaluation_context context, node_type subject) {
  try {
    return factory::create_int(win_registry::parse_type(subject->get_string_value(context)));
  } catch (const std::exception &e) {
    context->error(e.what());
    return factory::create_false();
  }
}

long long parse_type_name(const std::string &s) { return win_registry::parse_type(s); }

filter_obj_handler::filter_obj_handler() {
  // String attributes
  registry_.add_string("path",       &filter_obj::get_path,       "Full registry key path including hive (e.g. HKLM\\Software\\MyApp)")
            .add_string("name",       &filter_obj::get_name,       "Leaf key name")
            .add_string("parent",     &filter_obj::get_parent,     "Parent key path (full, including hive)")
            .add_string("hive",       &filter_obj::get_hive,       "Hive abbreviation (HKLM, HKCU, HKCR, HKU, HKCC)")
            .add_string("class",      &filter_obj::get_class_name, "Key class string (rarely set)")
            .add_string("written_s",  &filter_obj::get_written_s,  "Last-write time as a human-readable string");

  // Integer / boolean attributes
  registry_.add_int_x("depth",        &filter_obj::get_depth,        "Depth below the starting key (0 = the key itself)")
            .add_int_x("exists",       type_bool, &filter_obj::get_exists, "Whether the key exists (true/false)")
            .add_int_x("value_count",  &filter_obj::get_value_count,  "Number of values in this key")
            .add_int_x("subkey_count", &filter_obj::get_subkey_count, "Number of immediate sub-keys")
            .add_int_x("written",      type_date, &filter_obj::get_written, "Last-write time (epoch seconds; supports date comparisons)")
            .add_int_x("age",          type_int,  &filter_obj::get_age,    "Seconds since the key was last written");
}

}  // namespace check_rk_filter
}  // namespace registry_key_checks

void registry_key_checks::check(const PB::Commands::QueryRequestMessage::Request &request,
                                  PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_rk_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  std::vector<std::string> keys, excludes;
  std::string computer, view;
  long long max_depth = -1;  // -1 = no recursion by default (just the key itself)
  bool recursive = false;

  filter_type filter;
  filter_helper.add_options("", "not exists", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax(
      "${status}: ${problem_list}",
      "${path}: exists=${exists}, subkeys=${subkey_count}, values=${value_count}",
      "${path}",
      "${status}: No registry keys found",
      "${status}: All %(count) registry key(s) are ok.");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("key",       po::value<std::vector<std::string>>(&keys),
                  "One or more registry key paths to check (e.g. HKLM\\Software\\MyApp). "
                  "Use '*' as a wildcard to enumerate all immediate sub-keys of a path.")
    ("exclude",   po::value<std::vector<std::string>>(&excludes),
                  "Registry key names to exclude from enumeration")
    ("computer",  po::value<std::string>(&computer),
                  "Remote computer to connect to (empty = local)")
    ("view",      po::value<std::string>(&view)->default_value("default"),
                  "Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)")
    ("recursive", po::bool_switch(&recursive),
                  "Recursively enumerate all sub-keys below each starting key")
    ("max-depth", po::value<long long>(&max_depth),
                  "Maximum recursion depth (requires --recursive; -1 = unlimited)")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  // If recursive is requested and no explicit max-depth given, enumerate all levels
  if (recursive && max_depth < 0) max_depth = -1;  // unlimited
  if (!recursive) max_depth = 0;  // depth=0 → only the key itself (no children)

  if (keys.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No key specified. Please provide at least one key= argument.");
  }

  if (!filter_helper.build_filter(filter)) return;

  const DWORD access_flags = win_registry::parse_view(view);

  for (const std::string &key_arg : keys) {
    // Split into hive + subpath
    win_registry::key_path_parts parts;
    try {
      parts = win_registry::parse_key_path(key_arg);
    } catch (const nsclient::nsclient_exception &e) {
      return nscapi::protobuf::functions::set_response_bad(*response, e.reason());
    }

    // Connect to remote registry if needed
    win_registry::raii_hkey remote_root;
    HKEY effective_root = parts.hive;
    if (!computer.empty()) {
      try {
        HKEY rr = win_registry::connect_registry(computer, parts.hive);
        remote_root.hKey = rr;
        effective_root = rr;
      } catch (const win_registry::registry_exception &e) {
        return nscapi::protobuf::functions::set_response_bad(*response, e.reason());
      }
    }

    const std::string hive_str = parts.hive_str;

    if (recursive) {
      // Enumerate sub-keys recursively
      std::vector<win_registry::key_info> found;
      win_registry::recursive_enum_keys(effective_root, parts.subpath, key_arg, hive_str, 1, max_depth, access_flags, found);
      for (const win_registry::key_info &ki : found) {
        if (std::find(excludes.begin(), excludes.end(), ki.name) != excludes.end()) continue;
        boost::shared_ptr<win_registry::key_info> record(new win_registry::key_info(ki));
        filter.match(record);
        if (filter.has_errors()) return nscapi::protobuf::functions::set_response_bad(*response, "Filter error: " + filter.get_errors());
      }
    } else {
      // Query the specific key itself (depth = 0)
      const auto last_sep = key_arg.rfind('\\');
      const std::string key_name   = (last_sep == std::string::npos) ? key_arg : key_arg.substr(last_sep + 1);
      const std::string key_parent = (last_sep == std::string::npos) ? std::string() : key_arg.substr(0, last_sep);

      win_registry::key_info ki = win_registry::open_key(
          effective_root, parts.subpath, key_arg, key_name, key_parent, hive_str, 0, access_flags);

      boost::shared_ptr<win_registry::key_info> record(new win_registry::key_info(ki));
      filter.match(record);
      if (filter.has_errors()) return nscapi::protobuf::functions::set_response_bad(*response, "Filter error: " + filter.get_errors());
    }
  }

  filter_helper.post_process(filter);
}

// ══════════════════════════════════════════════════════════════════════════════
//  check_registry_value
// ══════════════════════════════════════════════════════════════════════════════

namespace registry_value_checks {
namespace check_rv_filter {

using namespace parsers::where;

// Converter: allow filter expressions like  type = 'REG_DWORD'
node_type parse_type_converter(boost::shared_ptr<filter_obj> /*obj*/, evaluation_context context, node_type subject) {
  try {
    return factory::create_int(win_registry::parse_type(subject->get_string_value(context)));
  } catch (const std::exception &e) {
    context->error(e.what());
    return factory::create_false();
  }
}

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_reg_type = type_custom_int_1;

  // String attributes
  registry_.add_string("key",          &filter_obj::get_key,          "Parent key path (full, including hive)")
            .add_string("name",         &filter_obj::get_name,         "Value name ('(default)' for the unnamed default value)")
            .add_string("path",         &filter_obj::get_path,         "Full path: key\\name")
            .add_string("hive",         &filter_obj::get_hive,         "Hive abbreviation (HKLM, HKCU, HKCR, HKU, HKCC)")
            .add_string("string_value", &filter_obj::get_string_value, "Value rendered as a string (REG_SZ expanded, REG_DWORD as decimal, REG_BINARY as hex, etc.)")
            .add_string("written_s",    &filter_obj::get_written_s,    "Parent key last-write time as a human-readable string");

  // Integer / custom-type attributes
  registry_.add_int_x("type",      type_custom_reg_type, &filter_obj::get_type_i, "Value type (REG_SZ, REG_DWORD, etc.)")
            .add_int_x("int_value", type_int,             &filter_obj::get_int_value, "Numeric value (REG_DWORD / REG_QWORD); 0 for non-numeric types")
            .add_int_x("size",      type_size,            &filter_obj::get_size,      "Raw byte size of the value data")
            .add_int_x("exists",    type_bool,            &filter_obj::get_exists,    "Whether the value exists (true/false)")
            .add_int_x("written",   type_date,            &filter_obj::get_written,   "Parent key last-write time (epoch seconds; supports date comparisons)")
            .add_int_x("age",       type_int,             &filter_obj::get_age,       "Seconds since parent key was last written");

  // Human-readable alias for 'type'
  registry_.add_human_string("type", &filter_obj::get_type_s, "Value type as a string (REG_SZ, REG_DWORD, etc.)");

  // Converter so that  type = 'REG_SZ'  works
  registry_.add_converter()(type_custom_reg_type, &parse_type_converter);
}

}  // namespace check_rv_filter
}  // namespace registry_value_checks

void registry_value_checks::check(const PB::Commands::QueryRequestMessage::Request &request,
                                   PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_rv_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  std::vector<std::string> keys, value_names, excludes;
  std::string computer, view;
  long long max_depth = 0;  // 0 = only the specified key (no recursion)
  bool recursive = false;

  filter_type filter;
  filter_helper.add_options("", "not exists", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax(
      "${status}: ${problem_list}",
      "${path}: ${string_value} (type=${type})",
      "${path}",
      "${status}: No registry values found",
      "${status}: All %(count) registry value(s) are ok.");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("key",       po::value<std::vector<std::string>>(&keys),
                  "One or more registry key paths whose values to check (e.g. HKLM\\Software\\MyApp)")
    ("value",     po::value<std::vector<std::string>>(&value_names),
                  "Restrict to specific value names (default: all values). Supports '*' to enumerate all.")
    ("exclude",   po::value<std::vector<std::string>>(&excludes),
                  "Value names to exclude from enumeration")
    ("computer",  po::value<std::string>(&computer),
                  "Remote computer to connect to (empty = local)")
    ("view",      po::value<std::string>(&view)->default_value("default"),
                  "Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)")
    ("recursive", po::bool_switch(&recursive),
                  "Recursively enumerate values in all sub-keys")
    ("max-depth", po::value<long long>(&max_depth),
                  "Maximum recursion depth for --recursive (-1 = unlimited)")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (recursive && max_depth == 0) max_depth = -1;  // unlimited when recursive without explicit depth

  if (keys.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No key specified. Please provide at least one key= argument.");
  }

  if (!filter_helper.build_filter(filter)) return;

  const DWORD access_flags = win_registry::parse_view(view);
  const bool all_values = value_names.empty() || (value_names.size() == 1 && value_names.front() == "*");

  for (const std::string &key_arg : keys) {
    win_registry::key_path_parts parts;
    try {
      parts = win_registry::parse_key_path(key_arg);
    } catch (const nsclient::nsclient_exception &e) {
      return nscapi::protobuf::functions::set_response_bad(*response, e.reason());
    }

    win_registry::raii_hkey remote_root;
    HKEY effective_root = parts.hive;
    if (!computer.empty()) {
      try {
        HKEY rr = win_registry::connect_registry(computer, parts.hive);
        remote_root.hKey = rr;
        effective_root = rr;
      } catch (const win_registry::registry_exception &e) {
        return nscapi::protobuf::functions::set_response_bad(*response, e.reason());
      }
    }

    const std::string hive_str = parts.hive_str;

    auto process_values = [&](const std::string &subpath, const std::string &full_path) {
      if (all_values) {
        // Enumerate all values in this key
        const std::vector<win_registry::value_info> vals = win_registry::enum_values(effective_root, subpath, full_path, hive_str, access_flags);
        for (const win_registry::value_info &vi : vals) {
          if (std::find(excludes.begin(), excludes.end(), vi.name) != excludes.end()) continue;
          boost::shared_ptr<win_registry::value_info> record(new win_registry::value_info(vi));
          filter.match(record);
          if (filter.has_errors()) return false;
        }
      } else {
        // Check specific named values

        // Open the key once to read its last-write time, then close it
        unsigned long long written_ft = 0;
        const std::wstring wsp = utf8::cvt<std::wstring>(subpath);
        {
          HKEY hTmp = NULL;
          if (RegOpenKeyExW(effective_root, wsp.empty() ? nullptr : wsp.c_str(), 0, KEY_QUERY_VALUE | KEY_READ | access_flags, &hTmp) == ERROR_SUCCESS) {
            FILETIME ft = {};
            RegQueryInfoKeyW(hTmp, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &ft);
            written_ft = win_registry::filetime_to_ull(ft);
            RegCloseKey(hTmp);
          }
        }

        for (const std::string &vname : value_names) {
          if (std::find(excludes.begin(), excludes.end(), vname) != excludes.end()) continue;

          win_registry::value_info vi;
          vi.key  = full_path;
          vi.name = vname;
          vi.path = full_path + "\\" + (vname.empty() ? "(default)" : vname);
          vi.hive = hive_str;
          vi.written_ft = written_ft;

          // Open key and read value
          HKEY hTmp = NULL;
          if (RegOpenKeyExW(effective_root, wsp.empty() ? nullptr : wsp.c_str(), 0, KEY_QUERY_VALUE | KEY_READ | access_flags, &hTmp) == ERROR_SUCCESS) {
            win_registry::detail::fill_value(hTmp, vname, written_ft, vi);
            RegCloseKey(hTmp);
          } else {
            vi.exists = false;
          }

          boost::shared_ptr<win_registry::value_info> record(new win_registry::value_info(vi));
          filter.match(record);
          if (filter.has_errors()) return false;
        }
      }
      return true;
    };

    // Helper to derive the subpath (relative to hive root) from a full path like "HKLM\Software\Foo"
    auto full_to_subpath = [&](const std::string &full_path) -> std::string {
      const std::size_t sep = full_path.find('\\');
      return (sep == std::string::npos) ? std::string() : full_path.substr(sep + 1);
    };

    if (recursive) {
      // Process values in the root key itself
      if (!process_values(parts.subpath, key_arg)) {
        return nscapi::protobuf::functions::set_response_bad(*response, "Filter error: " + filter.get_errors());
      }

      // Enumerate all descendant sub-keys and process their values too
      std::vector<win_registry::key_info> sub_keys;
      win_registry::recursive_enum_keys(effective_root, parts.subpath, key_arg, hive_str, 1, max_depth, access_flags, sub_keys);
      for (const win_registry::key_info &child : sub_keys) {
        if (!process_values(full_to_subpath(child.path), child.path)) {
          return nscapi::protobuf::functions::set_response_bad(*response, "Filter error: " + filter.get_errors());
        }
      }
    } else {
      if (!process_values(parts.subpath, key_arg)) {
        return nscapi::protobuf::functions::set_response_bad(*response, "Filter error: " + filter.get_errors());
      }
    }
  }

  filter_helper.post_process(filter);
}
