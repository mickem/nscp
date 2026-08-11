// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_installed_software.hpp"

// Windows.h must precede sddl.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <sddl.h>

// Older Windows SDKs (still used by the 32-bit build) do not define the ARM64
// architecture constant; the value is fixed by the platform ABI.
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

#include <boost/algorithm/string.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <win/registry.hpp>

#include "check_patch_age.hpp"

namespace installed_software_check {

using parsers::where::type_bool;
using parsers::where::type_date;
using parsers::where::type_size;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("name", &software_entry::get_name, "Product display name")
      .add_string_var("version", &software_entry::get_version, "Display version string (comparisons are lexical, not semver-aware)")
      .add_string_var("publisher", &software_entry::get_publisher, "Publisher / vendor")
      .add_string_var("install_date_s", &software_entry::get_install_date_str, "Raw InstallDate string as recorded (usually YYYYMMDD; often empty)")
      .add_string_var("install_location", &software_entry::get_install_location, "Install folder (InstallLocation)")
      .add_string_var("uninstall_string", &software_entry::get_uninstall_string, "Uninstall command line (UninstallString)")
      .add_string_var("hive", &software_entry::get_hive, "'machine' (HKLM) or 'user' (per-user install)")
      .add_string_var("user", &software_entry::get_user, "Account (or SID) owning a per-user install; empty for machine-wide")
      .add_string_var("architecture", &software_entry::get_architecture, "'x64' or 'x86' (registry view); empty for per-user installs")
      .add_string_var("key", &software_entry::get_key, "Uninstall registry sub-key name (product GUID or slug)");
  registry_.add_int_var("install_date", type_date, &software_entry::get_install_date,
                        "Install date (supports date expressions such as 'install_date > -30d'); unset when Windows did not record one")
      .add_int_var("size", type_size, &software_entry::get_size, "Estimated install size (from EstimatedSize); 0 when not recorded")
      .add_int_var("system_component", type_bool, &software_entry::get_system_component,
                   "True for entries flagged SystemComponent (hidden from Add/Remove Programs); excluded by the default filter")
      .add_int_var("windows_installer", type_bool, &software_entry::get_windows_installer, "True when the product was installed via Windows Installer (MSI)");
  // clang-format on
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const std::vector<software_entry> &entries) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  // The default filter hides SystemComponent entries, matching what Add/Remove
  // Programs shows. No default thresholds: a bare call is an inventory (OK +
  // count perf); unwanted/EOL policy comes in via warn=/crit= expressions, and
  // an empty match set (an absent-unwanted-software probe) is OK.
  filter_helper.add_options("", "", "system_component = 0", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_list}", "${name} ${version} (${publisher})", "${name}", "%(status): No installed software found",
                           "%(status): %(count) software packages installed.");
  // Thresholding on install_date must not spray a meaningless epoch-seconds
  // perf series per package; the aggregate count is the useful perf value.
  filter_helper.set_default_perf_config("install_date(ignored:true)extra(count)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  parsers::where::constants::reset();
  for (const software_entry &e : entries) {
    const std::shared_ptr<software_entry> record(new software_entry(e));
    filter.match(record);
    if (filter.has_errors()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Filter error: " + filter.get_errors());
    }
  }
  filter_helper.post_process(filter);
}

namespace {

const char *UNINSTALL_SUBPATH = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

bool os_is_64bit() {
  SYSTEM_INFO si = {};
  GetNativeSystemInfo(&si);
  return si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 || si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ||
         si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64;
}

// Best-effort SID → "DOMAIN\name"; falls back to the SID string itself.
std::string sid_to_account(const std::string &sid_str) {
  const std::wstring wsid = utf8::cvt<std::wstring>(sid_str);
  PSID psid = nullptr;
  if (!ConvertStringSidToSidW(wsid.c_str(), &psid)) return sid_str;
  wchar_t name[256];
  wchar_t domain[256];
  DWORD cch_name = 256;
  DWORD cch_domain = 256;
  SID_NAME_USE use;
  std::string ret = sid_str;
  if (LookupAccountSidW(nullptr, psid, name, &cch_name, domain, &cch_domain, &use)) {
    ret = utf8::cvt<std::string>(std::wstring(domain) + L"\\" + name);
  }
  LocalFree(psid);
  return ret;
}

// Enumerate one Uninstall key and append every product entry found in it.
void gather_from(HKEY root, const std::string &base_subpath, const DWORD view_flags, const std::string &hive, const std::string &user,
                 const std::string &architecture, std::vector<software_entry> &out) {
  HKEY base = nullptr;
  const std::wstring wbase = utf8::cvt<std::wstring>(base_subpath);
  // Missing base key (no per-user installs, no 32-bit view) is a normal state, not an error.
  if (RegOpenKeyExW(root, wbase.c_str(), 0, KEY_READ | view_flags, &base) != ERROR_SUCCESS) return;
  const win_registry::raii_hkey base_guard(base);

  DWORD num_subkeys = 0;
  DWORD max_name_len = 0;
  if (RegQueryInfoKeyW(base, nullptr, nullptr, nullptr, &num_subkeys, &max_name_len, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS ||
      num_subkeys == 0) {
    return;
  }

  std::vector<wchar_t> namebuf(max_name_len + 2);
  for (DWORD i = 0; i < num_subkeys; ++i) {
    DWORD cch = max_name_len + 1;
    if (RegEnumKeyExW(base, i, namebuf.data(), &cch, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) continue;
    const std::wstring child_name(namebuf.data(), cch);

    HKEY child = nullptr;
    if (RegOpenKeyExW(base, child_name.c_str(), 0, KEY_QUERY_VALUE | KEY_READ | view_flags, &child) != ERROR_SUCCESS) continue;
    const win_registry::raii_hkey child_guard(child);

    const auto value = [&child](const char *value_name) {
      win_registry::value_info vi;
      win_registry::detail::fill_value(child, value_name, 0, vi);
      return vi;
    };

    // No DisplayName → not a product entry (component GUIDs, orphaned keys).
    const win_registry::value_info display_name = value("DisplayName");
    if (!display_name.exists || display_name.string_value.empty()) continue;
    // ParentKeyName marks legacy patch/hotfix children (IE updates et al), not installed software.
    if (!value("ParentKeyName").string_value.empty()) continue;

    software_entry e;
    e.name = display_name.string_value;
    e.version = value("DisplayVersion").string_value;
    e.publisher = value("Publisher").string_value;
    const win_registry::value_info date = value("InstallDate");
    e.install_date_str = date.string_value;
    e.install_date_epoch = patch_age_check::parse_installed_on(date.string_value);
    e.install_location = value("InstallLocation").string_value;
    e.uninstall_string = value("UninstallString").string_value;
    e.size_kb = value("EstimatedSize").int_value;
    e.system_component = value("SystemComponent").int_value != 0;
    e.windows_installer = value("WindowsInstaller").int_value != 0;
    e.hive = hive;
    e.user = user;
    e.architecture = architecture;
    e.key = utf8::cvt<std::string>(child_name);
    out.push_back(e);
  }
}

}  // namespace

std::vector<software_entry> gather_installed_software() {
  std::vector<software_entry> out;

  if (os_is_64bit()) {
    gather_from(HKEY_LOCAL_MACHINE, UNINSTALL_SUBPATH, KEY_WOW64_64KEY, "machine", "", "x64", out);
    gather_from(HKEY_LOCAL_MACHINE, UNINSTALL_SUBPATH, KEY_WOW64_32KEY, "machine", "", "x86", out);
  } else {
    // 32-bit Windows has a single view; the WOW64 flags would alias it and duplicate every entry.
    gather_from(HKEY_LOCAL_MACHINE, UNINSTALL_SUBPATH, 0, "machine", "", "x86", out);
  }

  // Loaded per-user hives. Skip the *_Classes aliases and .DEFAULT (the same
  // hive as S-1-5-18, which is enumerated by SID). Per-user Software is not
  // WOW64-redirected, so a single default-view walk is complete.
  const std::vector<win_registry::key_info> users = win_registry::enum_sub_keys(HKEY_USERS, "", "HKU", "HKU", 1, 0);
  for (const win_registry::key_info &u : users) {
    if (u.name == ".DEFAULT" || boost::iends_with(u.name, "_Classes")) continue;
    gather_from(HKEY_USERS, u.name + "\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, "user", sid_to_account(u.name), "", out);
  }

  return out;
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_from(request, response, gather_installed_software());
  } catch (const win_registry::registry_exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to enumerate installed software: " + e.reason());
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to enumerate installed software: " + std::string(e.what()));
  }
}

}  // namespace installed_software_check
