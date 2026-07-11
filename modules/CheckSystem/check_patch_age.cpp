// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_patch_age.hpp"

#include <Windows.h>
#include <comdef.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <ctime>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <set>
#include <str/xtos.hpp>
#include <win/wmi/wmi_query.hpp>

namespace po = boost::program_options;

namespace patch_age_check {

namespace {

// Days from the civil date (Howard Hinnant's algorithm) to 1970-01-01. Valid
// for any Gregorian date; avoids a boost::date_time link dependency.
long long days_from_civil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

bool all_digits(const std::string &s) { return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; }); }

// Normalise a HotFixID for comparison: upper-case, and prepend "KB" when the
// request is a bare number (so hotfix=5034441 matches "KB5034441").
std::string normalize_kb(const std::string &s) {
  std::string t = boost::to_upper_copy(boost::trim_copy(s));
  if (all_digits(t)) t = "KB" + t;
  return t;
}

}  // namespace

long long parse_installed_on(const std::string &raw) {
  const std::string s = boost::trim_copy(raw);
  if (s.empty()) return 0;

  // "M/D/YYYY" or "MM/DD/YYYY" — the usual Win32_QuickFixEngineering form.
  if (s.find('/') != std::string::npos) {
    std::vector<std::string> parts;
    boost::split(parts, s, boost::is_any_of("/"));
    if (parts.size() == 3 && all_digits(parts[0]) && all_digits(parts[1]) && all_digits(parts[2])) {
      try {
        const int month = std::stoi(parts[0]);
        const int day = std::stoi(parts[1]);
        const int year = std::stoi(parts[2]);
        if (month >= 1 && month <= 12 && day >= 1 && day <= 31 && year >= 1980) {
          return days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400LL;
        }
      } catch (...) {
        return 0;
      }
    }
    return 0;
  }

  // "YYYYMMDD" — the compact 8-digit form some systems report.
  if (s.size() == 8 && all_digits(s)) {
    try {
      const int year = std::stoi(s.substr(0, 4));
      const int month = std::stoi(s.substr(4, 2));
      const int day = std::stoi(s.substr(6, 2));
      if (month >= 1 && month <= 12 && day >= 1 && day <= 31 && year >= 1980) {
        return days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400LL;
      }
    } catch (...) {
      return 0;
    }
  }

  return 0;
}

std::string patch_obj::get_message() const {
  std::string m;
  if (count == 0) {
    m = "No hotfixes reported";
  } else if (age_days >= 0) {
    m = str::xtos(count) + " hotfixes installed, newest " + newest_id + " on " + newest_installed + " (" + str::xtos(age_days) + "d ago)";
  } else {
    m = str::xtos(count) + " hotfixes installed, newest install date unknown";
  }
  if (missing > 0) m += "; missing: " + missing_ids;
  return m;
}

patch_obj build_patch_obj(const std::vector<hotfix_entry> &entries, const std::vector<std::string> &required, long long now_epoch) {
  patch_obj o;
  o.count = static_cast<long long>(entries.size());

  std::set<std::string> installed_norm;
  long long newest_epoch = 0;
  for (const hotfix_entry &e : entries) {
    if (!o.ids.empty()) o.ids += ";";
    o.ids += e.id;
    installed_norm.insert(normalize_kb(e.id));
    if (e.installed_epoch > newest_epoch) {
      newest_epoch = e.installed_epoch;
      o.newest_id = e.id;
      o.newest_installed = e.installed_str;
    }
  }

  if (newest_epoch > 0) {
    const long long secs = now_epoch - newest_epoch;
    o.age_days = secs > 0 ? secs / 86400LL : 0;
  } else {
    o.age_days = -1;
  }

  o.required = static_cast<long long>(required.size());
  for (const std::string &req : required) {
    if (installed_norm.find(normalize_kb(req)) == installed_norm.end()) {
      o.missing++;
      if (!o.missing_ids.empty()) o.missing_ids += ";";
      o.missing_ids += boost::trim_copy(req);
    }
  }

  return o;
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_var("count", &patch_obj::get_count, "Total number of installed hotfixes")
      .add_int_var("age", &patch_obj::get_age, "Days since the newest hotfix was installed (-1 if the install date is unknown)")
      .add_int_var("required", &patch_obj::get_required, "Number of hotfixes requested via the hotfix= option")
      .add_int_var("missing", &patch_obj::get_missing, "Number of requested hotfixes that are not installed");
  registry_.add_string_var("newest_id", &patch_obj::get_newest_id, "HotFixID of the most recently installed hotfix")
      .add_string_var("newest_installed", &patch_obj::get_newest_installed, "Install date of the newest hotfix (as reported by Windows)")
      .add_string_var("ids", &patch_obj::get_ids, "Semicolon-separated list of all installed HotFixIDs (use 'ids like KBxxxxxxx' to test presence)")
      .add_string_var("missing_ids", &patch_obj::get_missing_ids, "Semicolon-separated list of the requested hotfixes that are missing")
      .add_string_var("message", &patch_obj::get_message, "Full status sentence used as the default detail line");
  // clang-format on
}

void check_patch_age_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                          const std::vector<hotfix_entry> &entries, long long now_epoch) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  std::vector<std::string> required;

  filter_type filter;
  // Default critical trips only when a requested hotfix is missing; it is inert
  // when no hotfix= is given (missing is then always 0). Age thresholds are
  // opt-in via warn=/crit=. There is always exactly one row.
  filter_helper.add_options("", "missing > 0", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${message}", "patch", "", "");
  filter_helper.set_default_perf_config("extra(age;count;missing)");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("hotfix", po::value<std::vector<std::string>>(&required),
     "A required HotFixID (repeatable). The check is CRITICAL when a requested hotfix is not installed. "
     "A bare number is matched with an implicit 'KB' prefix (hotfix=5034441 == hotfix=KB5034441).")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const patch_obj data = build_patch_obj(entries, required, now_epoch);
  const std::shared_ptr<patch_obj> record(new patch_obj(data));
  filter.match(record);

  filter_helper.post_process(filter);
}

std::vector<hotfix_entry> gather_hotfixes() {
  std::vector<hotfix_entry> out;

  const HRESULT hr_init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  // SUCCEEDED covers S_OK/S_FALSE; RPC_E_CHANGED_MODE (COM already initialised
  // in another mode) fails SUCCEEDED, so we proceed without uninitialising.
  const bool needs_uninit = SUCCEEDED(hr_init);
  try {
    wmi_impl::query q("SELECT HotFixID, InstalledOn, Description FROM Win32_QuickFixEngineering", "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      hotfix_entry e;
      e.id = r.get_string("HotFixID");
      e.installed_str = r.get_string("InstalledOn");
      e.description = r.get_string("Description");
      e.installed_epoch = parse_installed_on(e.installed_str);
      if (e.id.empty()) continue;
      out.push_back(e);
    }
  } catch (...) {
    if (needs_uninit) CoUninitialize();
    throw;
  }
  if (needs_uninit) CoUninitialize();
  return out;
}

void check_patch_age(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    const std::vector<hotfix_entry> entries = gather_hotfixes();
    check_patch_age_from(request, response, entries, static_cast<long long>(std::time(nullptr)));
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to enumerate installed hotfixes: " + std::string(e.what()));
  }
}

}  // namespace patch_age_check
