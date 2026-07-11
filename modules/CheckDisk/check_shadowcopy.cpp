// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_shadowcopy.hpp"

#include <algorithm>
#include <ctime>
#include <map>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#ifdef WIN32
#include <win/wmi/wmi_query.hpp>
#endif

namespace shadowcopy_check {

namespace {

// Days from the civil date (Howard Hinnant's algorithm) to 1970-01-01.
long long days_from_civil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<long long>(doe) - 719468;
}

bool digits(const std::string &s, std::size_t off, std::size_t n) {
  if (off + n > s.size()) return false;
  for (std::size_t i = 0; i < n; ++i)
    if (s[off + i] < '0' || s[off + i] > '9') return false;
  return true;
}

int num(const std::string &s, std::size_t off, std::size_t n) { return std::stoi(s.substr(off, n)); }

}  // namespace

long long parse_cim_datetime(const std::string &s) {
  // CIM_DATETIME: yyyymmddHHMMSS.ffffff±UUU  (UUU = minutes offset from UTC).
  // We need at least the 14 leading date/time digits.
  if (s.size() < 14 || !digits(s, 0, 14)) return 0;
  try {
    const int year = num(s, 0, 4);
    const unsigned month = static_cast<unsigned>(num(s, 4, 2));
    const unsigned day = static_cast<unsigned>(num(s, 6, 2));
    const int hour = num(s, 8, 2);
    const int minute = num(s, 10, 2);
    const int second = num(s, 12, 2);
    if (year < 1980 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) return 0;

    long long epoch = days_from_civil(year, month, day) * 86400LL + hour * 3600LL + minute * 60LL + second;

    // Apply the UTC offset if present: the timestamp is local wall-clock, so
    // UTC = local - offset. Offset sits after the '.' and its 6 fractional
    // digits, as a signed 3-digit minute count (e.g. "-000", "+060").
    const std::size_t dot = s.find('.');
    if (dot != std::string::npos && dot + 7 < s.size()) {
      const char sign = s[dot + 7];
      if ((sign == '+' || sign == '-') && digits(s, dot + 8, 3)) {
        const int off_min = num(s, dot + 8, 3);
        epoch -= (sign == '-' ? -off_min : off_min) * 60LL;
      }
    }
    return epoch;
  } catch (...) {
    return 0;
  }
}

std::string volume_guid(const std::string &path) {
  const std::size_t start = path.find("Volume{");
  if (start == std::string::npos) return {};
  const std::size_t end = path.find('}', start);
  if (end == std::string::npos) return {};
  return path.substr(start, end - start + 1);
}

volumes_type build_volumes(const std::vector<raw_shadow> &shadows, const std::vector<raw_storage> &storages, long long now_epoch) {
  // Index shadow-storage usage by volume GUID for the join.
  std::map<std::string, const raw_storage *> storage_by_guid;
  for (const raw_storage &st : storages) {
    const std::string g = volume_guid(st.volume);
    if (!g.empty()) storage_by_guid[g] = &st;
  }

  // Group shadow copies by volume, preserving first-seen order.
  std::vector<std::string> order;
  std::map<std::string, shadowcopy> by_volume;
  for (const raw_shadow &sh : shadows) {
    auto it = by_volume.find(sh.volume);
    if (it == by_volume.end()) {
      shadowcopy v;
      v.volume = sh.volume;
      v.now_epoch = now_epoch;
      by_volume[sh.volume] = v;
      order.push_back(sh.volume);
      it = by_volume.find(sh.volume);
    }
    it->second.count++;
    if (sh.install_epoch > it->second.newest_epoch) it->second.newest_epoch = sh.install_epoch;
  }

  volumes_type out;
  for (const std::string &vol : order) {
    shadowcopy v = by_volume[vol];
    if (v.newest_epoch > 0) {
      const std::time_t t = static_cast<std::time_t>(v.newest_epoch);
      char buf[32] = {0};
      std::tm tmv{};
#ifdef _WIN32
      gmtime_s(&tmv, &t);
#else
      gmtime_r(&t, &tmv);
#endif
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmv);
      v.newest_date = buf;
    }
    const std::string g = volume_guid(vol);
    const auto sit = storage_by_guid.find(g);
    if (!g.empty() && sit != storage_by_guid.end()) {
      v.used = sit->second->used;
      v.allocated = sit->second->allocated;
      // MaxSpace of UINT64_MAX means "unbounded"; expose 0 so used_pct is inert.
      v.max_size = (sit->second->max_size < 0 || static_cast<unsigned long long>(sit->second->max_size) >= 0xFFFFFFFFFFFFF000ULL) ? 0 : sit->second->max_size;
    }
    out.push_back(v);
  }
  return out;
}

#ifdef WIN32
void query(std::vector<raw_shadow> &shadows, std::vector<raw_storage> &storages) {
  try {
    wmi_impl::query q("select VolumeName, InstallDate from Win32_ShadowCopy", "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      raw_shadow s;
      s.volume = r.get_string("VolumeName");
      s.install_epoch = parse_cim_datetime(r.get_string("InstallDate"));
      shadows.push_back(s);
    }
  } catch (const wmi_impl::wmi_exception &) {
    // No shadow copies / class unavailable: leave the list empty.
  }
  try {
    wmi_impl::query q("select Volume, UsedSpace, AllocatedSpace, MaxSpace from Win32_ShadowStorage", "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      raw_storage st;
      st.volume = r.get_string("Volume");
      st.used = r.get_int("UsedSpace");
      st.allocated = r.get_int("AllocatedSpace");
      st.max_size = r.get_int("MaxSpace");
      storages.push_back(st);
    }
  } catch (const wmi_impl::wmi_exception &) {
    // Shadow storage not configured: leave the list empty.
  }
}
#else
void query(std::vector<raw_shadow> &, std::vector<raw_storage> &) {}
#endif

namespace check {

typedef shadowcopy filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  using parsers::where::type_int;
  registry_.add_string_var("volume", &filter_obj::get_volume, "Volume the shadow copies belong to (VolumeName device path)")
      .add_string_var("newest_date", &filter_obj::get_newest_date, "Timestamp of the newest shadow copy on this volume (UTC)");
  // Distinct perf suffixes so co-referenced metrics each get their own series.
  registry_.add_int_var("count", &filter_obj::get_count, "Number of shadow copies on this volume")
      .add_int_perf("", "", "_count")
      .add_int_var("newest", type_int, &filter_obj::get_newest,
                   "Seconds since the newest shadow copy (-1 if unknown); threshold with durations, e.g. newest > 26h")
      .add_int_perf("s", "", "_newest")
      .add_int_var("used", &filter_obj::get_used, "Shadow storage used on this volume in bytes")
      .add_int_perf("B", "", "_used")
      .add_int_var("allocated", &filter_obj::get_allocated, "Shadow storage currently allocated in bytes")
      .no_perf()
      .add_int_var("max_size", &filter_obj::get_max_size, "Shadow storage maximum in bytes (0 if unbounded/unresolved)")
      .no_perf()
      .add_int_var("used_pct", &filter_obj::get_used_pct, "Percentage of the shadow-storage maximum in use")
      .add_int_perf("%", "", "_used_pct");
}

void check_shadowcopy(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // Default: alert when the newest snapshot is stale (assumes ~daily VSS
  // snapshots — adjust for weekly schedules). empty_state = "ok": a volume with
  // no shadow copies is common and not inherently a problem; require snapshots
  // explicitly with empty-state=critical.
  filter_helper.add_options("newest > 26h", "newest > 50h", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${volume}: ${count} copies, newest ${newest_date}", "${volume}", "%(status): No shadow copies found",
                           "%(status): All %(count) volume(s) have recent shadow copies.");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<raw_shadow> shadows;
  std::vector<raw_storage> storages;
  query(shadows, storages);

  for (const shadowcopy &v : build_volumes(shadows, storages, static_cast<long long>(std::time(nullptr)))) {
    std::shared_ptr<filter_obj> record(new filter_obj(v));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace shadowcopy_check
