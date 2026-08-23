// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pending_reboot.hpp"

#include <ctime>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <vector>
#include <win/registry.hpp>

#include "duration_keyword.hpp"

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

std::string reboot_obj::get_written_s() const {
  if (pending_since == 0) return "unknown";
  return str::format::format_date(static_cast<std::time_t>(pending_since));
}

std::string reboot_obj::get_message() const {
  if (!any()) return "No reboot pending";
  std::string msg = "Reboot required: " + get_reasons();
  if (pending_since != 0) msg += " (pending since " + get_written_s() + ")";
  return msg;
}

// age holds seconds; the converter lets `age > 7d` mean seven days instead of
// being silently read as the number 7.
static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_var("pending", parsers::where::type_bool, &reboot_obj::get_pending,
                        "1 if any pending-reboot signal is set (the aggregate flag most checks threshold on)")
      .add_int_var("signals", &reboot_obj::get_count, "Number of distinct pending-reboot signals currently set")
      .add_int_var("count", &reboot_obj::get_count, "Deprecated alias for signals (the name clashes with the generic count summary keyword).")
      .add_int_var("servicing", parsers::where::type_bool, &reboot_obj::get_servicing,
                   "1 if Component Based Servicing (CBS) has queued a reboot (the 'Component Based Servicing\\RebootPending' key exists)")
      .add_int_var("windows_update", parsers::where::type_bool, &reboot_obj::get_windows_update,
                   "1 if Windows Update has queued a reboot (WindowsUpdate\\Auto Update\\RebootRequired)")
      .add_int_var("file_rename", parsers::where::type_bool, &reboot_obj::get_file_rename,
                   "1 if 'Session Manager\\PendingFileRenameOperations' is present and non-empty (a file replacement awaits reboot)")
      .add_int_var("computer_rename", parsers::where::type_bool, &reboot_obj::get_computer_rename,
                   "1 if the computer has been renamed but not yet rebooted (ActiveComputerName differs from the pending ComputerName)")
      .add_int_var("domain_join", parsers::where::type_bool, &reboot_obj::get_domain_join,
                   "1 if a domain join / SPN update is pending in Netlogon (JoinDomain / AvoidSpnSet present)");
  // Only the CBS and Windows Update signals carry a timestamp (their keys exist
  // only while that reboot is queued), so written/age are optional: they render
  // as 'unknown', compare false against every number and emit no perfdata when
  // the pending reboot was signalled without one.
  registry_
      .add_optional_int_var("written", parsers::where::type_date, [](auto obj) { return obj->get_written(); }, "unknown",
                            "When the oldest timestamped pending-reboot signal appeared (last-write time of the CBS/Windows Update key; supports date "
                            "comparisons). 'unknown' when only untimestamped signals are set (`written = 'unknown'` tests for it)")
      .no_perf()
      .add_optional_int_var("age", type_custom_age, [](auto obj) { return obj->get_age(); }, "unknown",
                            "Seconds the reboot has been pending (since the oldest timestamped signal appeared); threshold with durations, e.g. "
                            "pending = 1 and age > 7d")
      .add_int_perf("s", "", "_age");
  registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<reboot_obj> >);
  registry_.add_string_var("written_s", &reboot_obj::get_written_s, "Signal-appearance time as a human-readable string ('unknown' if no timestamped signal)")
      .add_string_var("reasons", &reboot_obj::get_reasons, "Comma-separated human-readable list of pending-reboot causes ('none' if clear)")
      .add_string_var("message", &reboot_obj::get_message, "Full status sentence, e.g. 'Reboot required: Windows Update (pending since 2026-08-16 09:00:00)'");
  // clang-format on
}

namespace {

using win_registry::value_info;

// True if the key exists in the 64-bit registry view. When it does, *written
// receives the key's last-write time as epoch seconds (0 if the query fails).
bool key_exists(HKEY root, const std::string &subpath, long long *written = nullptr) {
  HKEY h = nullptr;
  const std::wstring w = utf8::cvt<std::wstring>(subpath);
  const LSTATUS r = RegOpenKeyExW(root, w.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &h);
  if (r == ERROR_SUCCESS) {
    if (written != nullptr) {
      FILETIME last_write = {};
      if (RegQueryInfoKeyW(h, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &last_write) == ERROR_SUCCESS)
        *written = win_registry::filetime_to_epoch(win_registry::filetime_to_ull(last_write));
    }
    RegCloseKey(h);
    return true;
  }
  return false;
}

// The oldest (earliest) of the timestamped signals: how long *a* reboot has
// been pending when more than one subsystem queued one.
long long oldest_written(long long a, long long b) {
  if (a == 0) return b;
  if (b == 0) return a;
  return a < b ? a : b;
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

  // Component Based Servicing: the key exists only while a servicing reboot is
  // queued, so its last-write time is when the signal appeared.
  long long servicing_written = 0;
  o.servicing = key_exists(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending", &servicing_written);

  // Windows Update: RebootRequired appears once WU queues a reboot (even for
  // updates already installed). Same key check_os_updates surfaces as reboot_pending.
  long long wu_written = 0;
  o.windows_update = key_exists(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired", &wu_written);

  o.pending_since = oldest_written(servicing_written, wu_written);
  o.now = static_cast<long long>(time(nullptr));

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
  filter_helper.set_default_perf_config("extra(pending;signals)");

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
