// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_counters.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_options.hpp"

namespace check_mssql_counters_command {

namespace {

// Most of these counters are cumulative since instance start, so a single read
// is meaningless as a rate. Two snapshots bracket a WAITFOR DELAY sampling
// window (kept server-side so client latency does not stretch it), and the
// window is measured rather than assumed. object_name carries the instance
// prefix (SQLServer: or MSSQL$<instance>:) and the columns are nchar-padded,
// hence the RTRIM + LIKE '%:...' matching. Locks counters exist per lock type;
// only the _Total instance is wanted.
const char *COUNTERS_SQL =
    "SET NOCOUNT ON;"
    " DECLARE @t0 datetime2 = SYSDATETIME();"
    " DECLARE @c1 TABLE(counter_name nvarchar(128) PRIMARY KEY, cntr_value bigint);"
    " INSERT INTO @c1 SELECT RTRIM(counter_name), cntr_value FROM sys.dm_os_performance_counters"
    " WHERE (RTRIM(object_name) LIKE '%:Buffer Manager' AND RTRIM(counter_name) IN"
    " ('Buffer cache hit ratio', 'Buffer cache hit ratio base', 'Page life expectancy', 'Lazy writes/sec'))"
    " OR (RTRIM(object_name) LIKE '%:SQL Statistics' AND RTRIM(counter_name) IN"
    " ('Batch Requests/sec', 'SQL Compilations/sec', 'SQL Re-Compilations/sec'))"
    " OR (RTRIM(object_name) LIKE '%:Locks' AND RTRIM(instance_name) = '_Total' AND RTRIM(counter_name) IN"
    " ('Number of Deadlocks/sec', 'Lock Waits/sec'));"
    " WAITFOR DELAY '00:00:01';"
    " SELECT RTRIM(pc.counter_name) AS counter_name, pc.cntr_value AS value, c1.cntr_value AS prev_value,"
    " DATEDIFF(millisecond, @t0, SYSDATETIME()) AS elapsed_ms"
    " FROM sys.dm_os_performance_counters pc"
    " LEFT JOIN @c1 c1 ON c1.counter_name = RTRIM(pc.counter_name)"
    " WHERE (RTRIM(pc.object_name) LIKE '%:Buffer Manager' AND RTRIM(pc.counter_name) IN"
    " ('Buffer cache hit ratio', 'Buffer cache hit ratio base', 'Page life expectancy', 'Lazy writes/sec'))"
    " OR (RTRIM(pc.object_name) LIKE '%:SQL Statistics' AND RTRIM(pc.counter_name) IN"
    " ('Batch Requests/sec', 'SQL Compilations/sec', 'SQL Re-Compilations/sec'))"
    " OR (RTRIM(pc.object_name) LIKE '%:Locks' AND RTRIM(pc.instance_name) = '_Total' AND RTRIM(pc.counter_name) IN"
    " ('Number of Deadlocks/sec', 'Lock Waits/sec'))";

typedef counters_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_float("hit_ratio", &filter_obj::get_hit_ratio, "Buffer cache hit ratio in percent over the sampling window (-1 if unavailable)")
      .add_float_perf("%", "", "_hit_ratio")
      .add_float("batch_requests", &filter_obj::get_batch_requests, "Batch requests per second (-1 if unavailable)")
      .add_float_perf("", "", "_batch_requests")
      .add_float("compilations", &filter_obj::get_compilations, "SQL compilations per second (-1 if unavailable)")
      .add_float_perf("", "", "_compilations")
      .add_float("recompilations", &filter_obj::get_recompilations, "SQL re-compilations per second (-1 if unavailable)")
      .add_float_perf("", "", "_recompilations")
      .add_float("lazy_writes", &filter_obj::get_lazy_writes, "Lazy writer pages flushed per second, sustained values mean memory pressure (-1 if unavailable)")
      .add_float_perf("", "", "_lazy_writes")
      .add_float("deadlocks", &filter_obj::get_deadlocks, "Deadlocks per second across all lock types (-1 if unavailable)")
      .add_float_perf("", "", "_deadlocks")
      .add_float("lock_waits", &filter_obj::get_lock_waits, "Lock requests per second that had to wait (-1 if unavailable)")
      .add_float_perf("", "", "_lock_waits");
  registry_
      .add_int_var("page_life_expectancy", &filter_obj::get_page_life_expectancy,
                   "Seconds a page stays in the buffer pool without being referenced (-1 if unavailable)")
      .add_int_perf("s", "", "_page_life_expectancy");
}

double rate(long long value, bool has_prev, long long prev, long long elapsed_ms) {
  if (!has_prev || elapsed_ms <= 0) return -1;
  return static_cast<double>(value - prev) * 1000.0 / static_cast<double>(elapsed_ms);
}

}  // namespace

counters_info build_counters(const std::vector<counter_row> &rows) {
  counters_info info;
  long long hit_value = -1, hit_base = -1, hit_value_prev = -1, hit_base_prev = -1;
  bool has_hit_prev = false, has_base_prev = false;
  for (const counter_row &row : rows) {
    if (row.name == "Buffer cache hit ratio") {
      hit_value = row.value;
      has_hit_prev = row.has_prev;
      hit_value_prev = row.prev_value;
    } else if (row.name == "Buffer cache hit ratio base") {
      hit_base = row.value;
      has_base_prev = row.has_prev;
      hit_base_prev = row.prev_value;
    } else if (row.name == "Page life expectancy") {
      info.page_life_expectancy = row.value;
    } else if (row.name == "Batch Requests/sec") {
      info.batch_requests = rate(row.value, row.has_prev, row.prev_value, row.elapsed_ms);
    } else if (row.name == "SQL Compilations/sec") {
      info.compilations = rate(row.value, row.has_prev, row.prev_value, row.elapsed_ms);
    } else if (row.name == "SQL Re-Compilations/sec") {
      info.recompilations = rate(row.value, row.has_prev, row.prev_value, row.elapsed_ms);
    } else if (row.name == "Lazy writes/sec") {
      info.lazy_writes = rate(row.value, row.has_prev, row.prev_value, row.elapsed_ms);
    } else if (row.name == "Number of Deadlocks/sec") {
      info.deadlocks = rate(row.value, row.has_prev, row.prev_value, row.elapsed_ms);
    } else if (row.name == "Lock Waits/sec") {
      info.lock_waits = rate(row.value, row.has_prev, row.prev_value, row.elapsed_ms);
    }
  }
  if (hit_value >= 0 && hit_base > 0) {
    if (has_hit_prev && has_base_prev && hit_base - hit_base_prev > 0) {
      // Ratio over the sampling window: the lifetime ratio converges to ~100%
      // on a long-running instance and hides a cold or thrashing cache.
      info.hit_ratio = 100.0 * static_cast<double>(hit_value - hit_value_prev) / static_cast<double>(hit_base - hit_base_prev);
    } else {
      // Nothing touched the cache during the window: fall back to lifetime.
      info.hit_ratio = 100.0 * static_cast<double>(hit_value) / static_cast<double>(hit_base);
    }
  }
  return info;
}

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;

  filter_type filter;
  // No default thresholds: healthy values are hardware- and workload-specific
  // (PLE scales with buffer pool size, batch rates with load).
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "hit ratio ${hit_ratio}%, PLE ${page_life_expectancy}s, ${batch_requests} batches/s, ${compilations} compilations/s, "
                           "${lazy_writes} lazy writes/s, ${lock_waits} lock waits/s, ${deadlocks} deadlocks/s",
                           "mssql", "%(status): No performance counters found", "");
  // A counters check is only useful if it graphs: emit every counter as
  // perfdata even when no threshold references it.
  filter_helper.set_default_perf_config("extra(hit_ratio;page_life_expectancy;batch_requests;compilations;recompilations;lazy_writes;deadlocks;lock_waits)");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(COUNTERS_SQL);
    std::vector<counter_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      counter_row row;
      row.name = res.get_string(i, "counter_name");
      row.value = res.get_int(i, "value");
      row.has_prev = !res.is_null(i, "prev_value");
      if (row.has_prev) row.prev_value = res.get_int(i, "prev_value");
      row.elapsed_ms = res.get_int(i, "elapsed_ms");
      rows.push_back(row);
    }

    if (!rows.empty()) {
      auto record = std::make_shared<filter_obj>(build_counters(rows));
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_counters_command
