// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pending_reboot.hpp"

#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/utf8.hpp>
#include <vector>
#include <win/registry.hpp>

namespace pending_reboot_check {

std::string reboot_obj::get_reasons() const {
  std::vector<std::string> parts;
  if (servicing) parts.push_back("Component Based Servicing");
  if (windows_update) parts.push_back("Windows Update");
  if (file_rename) parts.push_back("pending file rename");
  if (computer_rename) parts.push_back("computer rename");
  if (domain_join) parts.push_back("domain join");
  if (parts.empty()) return "none";
  std::string ret;
  for (const std::string &p : parts) {
    if (!ret.empty()) ret += ", ";
    ret += p;
  }
  return ret;
}

std::string reboot_obj::get_message() const {
  if (!any()) return "No reboot pending";
  return "Reboot required: " + get_reasons();
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_var("pending", parsers::where::type_bool, &reboot_obj::get_pending,
                        "1 if any pending-reboot signal is set (the aggregate flag most checks threshold on)")
      .add_int_var("count", &reboot_obj::get_count, "Number of distinct pending-reboot signals currently set")
      .add_int_var("servicing", parsers::where::type_bool, &reboot_obj::get_servicing,
                   "1 if Component Based Servicing (CBS) has queued a reboot")
      .add_int_var("windows_update", parsers::where::type_bool, &reboot_obj::get_windows_update,
                   "1 if Windows Update has queued a reboot (WindowsUpdate\\Auto Update\\RebootRequired)")
      .add_int_var("file_rename", parsers::where::type_bool, &reboot_obj::get_file_rename,
                   "1 if PendingFileRenameOperations is queued (a file replacement awaits reboot)")
      .add_int_var("computer_rename", parsers::where::type_bool, &reboot_obj::get_computer_rename,
                   "1 if the computer has been renamed but not yet rebooted")
      .add_int_var("domain_join", parsers::where::type_bool, &reboot_obj::get_domain_join,
                   "1 if a domain join / SPN update is pending in Netlogon");
  registry_.add_string_var("reasons", &reboot_obj::get_reasons, "Comma-separated human-readable list of pending-reboot causes ('none' if clear)")
      .add_string_var("message", &reboot_obj::get_message, "Full status sentence, e.g. 'Reboot required: Windows Update'");
  // clang-format on
}

namespace {

using win_registry::value_info;

// True if the key exists in the 64-bit registry view.
bool key_exists(HKEY root, const std::string &subpath) {
  HKEY h = nullptr;
  const std::wstring w = utf8::cvt<std::wstring>(subpath);
  const LSTATUS r = RegOpenKeyExW(root, w.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &h);
  if (r == ERROR_SUCCESS) {
    RegCloseKey(h);
    return true;
  }
  return false;
}

// True if a named value exists (and, for the file-rename list, is non-empty).
bool value_present(HKEY root, const std::string &subpath, const std::string &name, bool require_non_empty) {
  const value_info vi = win_registry::read_value(root, subpath, name, KEY_WOW64_64KEY);
  if (!vi.exists) return false;
  if (require_non_empty && vi.string_value.empty()) return false;
  return true;
}

}  // namespace

reboot_obj gather_pending_reboot() {
  reboot_obj o;

  // Component Based Servicing: the key exists only while a servicing reboot is queued.
  o.servicing = key_exists(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending");

  // Windows Update: RebootRequired appears once WU queues a reboot (even for
  // updates already installed). Same key check_os_updates surfaces as reboot_pending.
  o.windows_update = key_exists(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired");

  // PendingFileRenameOperations (REG_MULTI_SZ): a file replacement scheduled for
  // the next boot. Present-but-empty does not count.
  o.file_rename = value_present(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager", "PendingFileRenameOperations", true);

  // Computer rename: the active name differs from the (pending) configured name.
  {
    const value_info active =
        win_registry::read_value(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName", "ComputerName", KEY_WOW64_64KEY);
    const value_info pending =
        win_registry::read_value(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName", "ComputerName", KEY_WOW64_64KEY);
    if (active.exists && pending.exists && active.string_value != pending.string_value) o.computer_rename = true;
  }

  // Netlogon: a queued domain join or SPN update requires a reboot to complete.
  o.domain_join = value_present(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\Netlogon", "JoinDomain", false) ||
                  value_present(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\Netlogon", "AvoidSpnSet", false);

  return o;
}

void check_pending_reboot_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                               reboot_obj data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // Default: WARNING when any reboot is pending, no critical. There is always
  // exactly one row, so the empty-state never applies.
  filter_helper.add_options("pending = 1", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${message}", "reboot", "", "%(status): No reboot pending");
  filter_helper.set_default_perf_config("extra(pending;count)");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<reboot_obj> record(new reboot_obj(std::move(data)));
  filter.match(record);

  filter_helper.post_process(filter);
}

void check_pending_reboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_pending_reboot_from(request, response, gather_pending_reboot());
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read pending-reboot state: " + std::string(e.what()));
  }
}

}  // namespace pending_reboot_check
