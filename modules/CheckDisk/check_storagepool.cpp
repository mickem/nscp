// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_storagepool.hpp"

#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#ifdef WIN32
#include <win/wmi/wmi_query.hpp>
#endif

namespace storagepool_check {

namespace {
// Synthesise a single operational-status string from the scalar flags, since
// MSFT_StoragePool.OperationalStatus is a uint16[] that is awkward to threshold.
std::string derive_operational_status(const std::string &health, bool is_readonly) {
  if (is_readonly) return "ReadOnly";
  if (health == "Healthy" || health.empty()) return "OK";
  return health;
}
}  // namespace

#ifdef WIN32
pools_type query() {
  pools_type pools;
  try {
    // Skip the primordial pool (the reservoir of unpooled disks) — it is not a
    // real Storage Space and has no meaningful health/capacity to alert on.
    wmi_impl::query q("select FriendlyName, HealthStatus, Size, AllocatedSize, IsReadOnly from MSFT_StoragePool where IsPrimordial=FALSE",
                      "root\\Microsoft\\Windows\\Storage", "", "");
    wmi_impl::row_enumerator row = q.execute();
    while (row.has_next()) {
      const wmi_impl::row r = row.get_next();
      storagepool p;
      p.name = r.get_string("FriendlyName");
      p.health_status = storagepool::map_health_status(r.get_int("HealthStatus"));
      p.capacity = r.get_int("Size");
      p.used = r.get_int("AllocatedSize");
      p.is_readonly = r.get_int("IsReadOnly") != 0;
      p.operational_status = derive_operational_status(p.health_status, p.is_readonly);
      pools.push_back(p);
    }
  } catch (const wmi_impl::wmi_exception &) {
    // Storage namespace / MSFT_StoragePool unavailable (no Storage Spaces, older
    // Windows): report no pools rather than failing.
  }
  return pools;
}
#else
pools_type query() { return {}; }
#endif

namespace check {

typedef storagepool filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Storage pool friendly name")
      .add_string_var("health_status", &filter_obj::get_health_status, "Pool health: Healthy, Warning, Unhealthy or Unknown")
      .add_string_var("operational_status", &filter_obj::get_operational_status, "Pool operational status: OK, ReadOnly, ...");

  // Distinct perf suffixes so co-referenced metrics each get their own series
  // ('<pool>_capacity', ...) instead of collapsing onto the shared ${name} alias.
  registry_.add_int_var("capacity", &filter_obj::get_capacity, "Total pool capacity in bytes")
      .add_int_perf("B", "", "_capacity")
      .add_int_var("used", &filter_obj::get_used, "Allocated (used) pool space in bytes")
      .add_int_perf("B", "", "_used")
      .add_int_var("free", &filter_obj::get_free, "Unallocated (free) pool space in bytes")
      .add_int_perf("B", "", "_free")
      .add_int_var("free_pct", &filter_obj::get_free_pct, "Percentage of free pool space")
      .add_int_perf("%", "", "_free_pct")
      .add_int_var("used_pct", &filter_obj::get_used_pct, "Percentage of used pool space")
      .add_int_perf("%", "", "_used_pct")
      .add_int_var("is_readonly", &filter_obj::get_is_readonly, "1 if the pool is read-only")
      .no_perf();
}

void check_storagepool(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  // Alert on pool health/capacity by default: WARNING on a Warning pool or low
  // free space, CRITICAL on an Unhealthy pool.
  // empty_state = "ok": a system with no Storage Spaces pools is not a problem,
  // so the check must not go critical simply because there are no pools.
  filter_helper.add_options("health_status = 'Warning' or free_pct < 20", "health_status = 'Unhealthy' or free_pct < 10", "",
                            filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${name}: ${health_status}, ${used}/${capacity} used", "${name}", "%(status): No storage pools found",
                           "%(status): All storage pools are healthy.");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  for (const storagepool &p : query()) {
    std::shared_ptr<filter_obj> record(new filter_obj(p));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace storagepool_check
