// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_waits.hpp"

#include <cstring>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_options.hpp"

namespace check_mssql_waits_command {

namespace {

// Scheduler pressure is point-in-time: runnable tasks have CPU work but no
// scheduler slot, work-queue tasks do not even have a worker (THREADPOOL
// starvation). scheduler_id >= 255 are hidden/internal schedulers (DAC etc.).
const char *SCHEDULERS_SQL =
    "SELECT COUNT(*) AS schedulers,"
    " ISNULL(SUM(runnable_tasks_count), 0) AS runnable_tasks,"
    " ISNULL(SUM(work_queue_count), 0) AS work_queue,"
    " ISNULL(SUM(active_workers_count), 0) AS workers"
    " FROM sys.dm_os_schedulers WHERE scheduler_id < 255 AND status = 'VISIBLE ONLINE'";

// sys.dm_os_wait_stats is cumulative since instance start, so two snapshots
// bracket a server-side WAITFOR DELAY window and only the deltas are
// returned. Rows whose wait time did not move are filtered out server-side;
// classification happens client-side where it is unit-testable.
const char *WAITS_SQL =
    "SET NOCOUNT ON;"
    " DECLARE @t0 datetime2 = SYSDATETIME();"
    " DECLARE @w1 TABLE(wait_type nvarchar(120) PRIMARY KEY, wait_ms bigint, signal_ms bigint);"
    " INSERT INTO @w1 SELECT wait_type, wait_time_ms, signal_wait_time_ms FROM sys.dm_os_wait_stats;"
    " WAITFOR DELAY '00:00:01';"
    " SELECT w2.wait_type,"
    " w2.wait_time_ms - ISNULL(w1.wait_ms, 0) AS wait_ms,"
    " w2.signal_wait_time_ms - ISNULL(w1.signal_ms, 0) AS signal_ms,"
    " DATEDIFF(millisecond, @t0, SYSDATETIME()) AS elapsed_ms"
    " FROM sys.dm_os_wait_stats w2"
    " LEFT JOIN @w1 w1 ON w1.wait_type = w2.wait_type"
    " WHERE w2.wait_time_ms > ISNULL(w1.wait_ms, 0)";

bool starts_with(const std::string &s, const char *prefix) { return s.compare(0, strlen(prefix), prefix) == 0; }

typedef waits_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_
      .add_int_var("runnable_tasks", &filter_obj::get_runnable_tasks,
                   "Tasks that have CPU work but are waiting for a scheduler slot; sustained values above the core count mean CPU pressure")
      .add_int_perf("", "", "_runnable_tasks")
      .add_int_var("work_queue", &filter_obj::get_work_queue, "Tasks queued with no worker thread at all (THREADPOOL starvation when > 0)")
      .add_int_perf("", "", "_work_queue")
      .add_int_var("schedulers", &filter_obj::get_schedulers, "Visible online schedulers (compare runnable_tasks against this)")
      .no_perf()
      .add_int_var("workers", &filter_obj::get_workers, "Active worker threads")
      .add_int_perf("", "", "_workers");

  registry_.add_float("cpu_waits", &filter_obj::get_cpu_waits, "CPU/parallelism wait ms per second (SOS_SCHEDULER_YIELD, THREADPOOL, CX*)")
      .add_float_perf("", "", "_cpu_waits")
      .add_float("io_waits", &filter_obj::get_io_waits, "Data-file I/O wait ms per second (PAGEIOLATCH_*, IO_COMPLETION, BACKUPIO)")
      .add_float_perf("", "", "_io_waits")
      .add_float("log_waits", &filter_obj::get_log_waits, "Transaction-log wait ms per second (WRITELOG, LOGBUFFER)")
      .add_float_perf("", "", "_log_waits")
      .add_float("lock_waits", &filter_obj::get_lock_waits, "Lock wait ms per second (LCK_M_*)")
      .add_float_perf("", "", "_lock_waits")
      .add_float("latch_waits", &filter_obj::get_latch_waits, "Latch wait ms per second (PAGELATCH_*, LATCH_*)")
      .add_float_perf("", "", "_latch_waits")
      .add_float("memory_waits", &filter_obj::get_memory_waits, "Memory wait ms per second (RESOURCE_SEMAPHORE*, CMEMTHREAD)")
      .add_float_perf("", "", "_memory_waits")
      .add_float("network_waits", &filter_obj::get_network_waits,
                 "Network wait ms per second (ASYNC_NETWORK_IO - usually the client not consuming results, not the network)")
      .add_float_perf("", "", "_network_waits")
      .add_float("other_waits", &filter_obj::get_other_waits, "Wait ms per second not covered by the categories (benign waits excluded)")
      .add_float_perf("", "", "_other_waits")
      .add_float("total_waits", &filter_obj::get_total_waits, "Total non-benign wait ms per second")
      .add_float_perf("", "", "_total_waits")
      .add_float("signal_wait_pct", &filter_obj::get_signal_wait_pct,
                 "Percent of wait time spent runnable, i.e. waiting for CPU after the resource arrived; sustained > 20-25% means CPU pressure (-1 when "
                 "nothing waited)")
      .add_float_perf("%", "", "_signal_wait_pct");
}

}  // namespace

std::string categorize_wait(const std::string &w) {
  // PREEMPTIVE_* covers SQLOS switching a worker to preemptive mode to call out
  // of the engine. Most of those calls idle-accumulate and are excluded with the
  // prefix below, but a handful are external stalls that the check exists to
  // surface, so they are carved back out here (the same treatment HADR_ gets).
  // Autogrow on slow storage, a backup hanging on a URL or a stalled domain
  // lookup would otherwise leave every category reading zero in the middle of
  // the incident.
  const bool preemptive_stall = w == "PREEMPTIVE_OS_WRITEFILEGATHER" ||   // file growth and zeroing
                                w == "PREEMPTIVE_OS_FLUSHFILEBUFFERS" ||  // checkpoint and backup flushes
                                w == "PREEMPTIVE_HTTP_REQUEST" ||         // backup to URL, external endpoints
                                w == "PREEMPTIVE_OS_AUTHENTICATIONOPS" || // domain and Kerberos lookups
                                w == "PREEMPTIVE_OS_CRYPTOPS" ||          // EKM and TDE key operations
                                w == "PREEMPTIVE_ODBCOPS" || w == "PREEMPTIVE_OLEDBOPS";  // linked servers

  // Idle/housekeeping waits that accumulate by design and would drown every
  // real signal (the usual suspects from the community benign-wait lists).
  // HADR_ deliberately gets an explicit allowlist rather than the prefix:
  // HADR_SYNC_COMMIT is the primary synchronous-AG commit-latency signal and
  // must keep counting (it lands in other_waits/total_waits), while the
  // listed HADR housekeeping waits idle-accumulate on every AG instance.
  if (starts_with(w, "SLEEP_") || starts_with(w, "BROKER_") || starts_with(w, "SQLTRACE_") || starts_with(w, "XE_") || starts_with(w, "FT_") ||
      starts_with(w, "QDS_") || starts_with(w, "HADR_FILESTREAM_") || w == "HADR_CLUSAPI_CALL" || w == "HADR_CLUSTER_INTEGRATION" ||
      w == "HADR_FAILOVER_PARTNER" || w == "HADR_LOGCAPTURE_WAIT" || w == "HADR_NOTIFICATION_DEQUEUE" || w == "HADR_TIMER_TASK" ||
      w == "HADR_WORK_QUEUE" || starts_with(w, "DBMIRROR") || (starts_with(w, "PREEMPTIVE_") && !preemptive_stall) ||
      starts_with(w, "PARALLEL_REDO_") || starts_with(w, "PWAIT_") || starts_with(w, "SP_SERVER_DIAGNOSTICS") || starts_with(w, "VDI_CLIENT_") ||
      starts_with(w, "WAIT_XTP_") || w == "LAZYWRITER_SLEEP" || w == "LOGMGR_QUEUE" || w == "CHECKPOINT_QUEUE" || w == "REQUEST_FOR_DEADLOCK_SEARCH" ||
      w == "WAITFOR" || w == "WAITFOR_TASKSHUTDOWN" || w == "ONDEMAND_TASK_QUEUE" || w == "DIRTY_PAGE_POLL" || w == "SOS_WORK_DISPATCHER" ||
      w == "SLEEP_TASK" || w == "SERVER_IDLE_CHECK" || w == "XTP_HOST_WAIT" || w == "POPULATE_LOCK_ORDINALS" || w == "KSOURCE_WAKEUP" ||
      w == "TRACEWRITE" || w == "WINFAB_API_CALL")
    return "benign";
  if (w == "SOS_SCHEDULER_YIELD" || w == "THREADPOOL" || starts_with(w, "CX")) return "cpu";
  // The two file-level preemptive stalls are storage waits: report them where a
  // DBA looks for storage trouble rather than in the other_waits bucket.
  if (starts_with(w, "PAGEIOLATCH_") || w == "IO_COMPLETION" || w == "ASYNC_IO_COMPLETION" || w == "BACKUPIO" || w == "WRITE_COMPLETION" ||
      w == "PREEMPTIVE_OS_WRITEFILEGATHER" || w == "PREEMPTIVE_OS_FLUSHFILEBUFFERS")
    return "io";
  if (w == "WRITELOG" || w == "LOGBUFFER") return "log";
  if (starts_with(w, "LCK_M_")) return "lock";
  if (starts_with(w, "PAGELATCH_") || starts_with(w, "LATCH_")) return "latch";
  if (starts_with(w, "RESOURCE_SEMAPHORE") || w == "CMEMTHREAD") return "memory";
  if (w == "ASYNC_NETWORK_IO" || w == "NET_WAITFOR_PACKET") return "network";
  return "other";
}

waits_info build_waits(const std::vector<wait_row> &rows) {
  waits_info info;
  long long wait_total = 0, signal_total = 0, elapsed_ms = 0;
  double cpu = 0, io = 0, log = 0, lock = 0, latch = 0, memory = 0, network = 0, other = 0;
  for (const wait_row &row : rows) {
    const std::string category = categorize_wait(row.wait_type);
    if (category == "benign") continue;
    if (row.elapsed_ms > 0) elapsed_ms = row.elapsed_ms;
    wait_total += row.wait_ms;
    signal_total += row.signal_ms;
    if (category == "cpu")
      cpu += static_cast<double>(row.wait_ms);
    else if (category == "io")
      io += static_cast<double>(row.wait_ms);
    else if (category == "log")
      log += static_cast<double>(row.wait_ms);
    else if (category == "lock")
      lock += static_cast<double>(row.wait_ms);
    else if (category == "latch")
      latch += static_cast<double>(row.wait_ms);
    else if (category == "memory")
      memory += static_cast<double>(row.wait_ms);
    else if (category == "network")
      network += static_cast<double>(row.wait_ms);
    else
      other += static_cast<double>(row.wait_ms);
  }
  if (elapsed_ms > 0) {
    const double per_second = 1000.0 / static_cast<double>(elapsed_ms);
    info.cpu_waits = cpu * per_second;
    info.io_waits = io * per_second;
    info.log_waits = log * per_second;
    info.lock_waits = lock * per_second;
    info.latch_waits = latch * per_second;
    info.memory_waits = memory * per_second;
    info.network_waits = network * per_second;
    info.other_waits = other * per_second;
    info.total_waits = static_cast<double>(wait_total) * per_second;
  }
  if (wait_total > 0) info.signal_wait_pct = 100.0 * static_cast<double>(signal_total) / static_cast<double>(wait_total);
  return info;
}

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;

  filter_type filter;
  // No default thresholds: wait rates only mean something against the
  // workload's own baseline. work_queue > 0 and runnable_tasks > cores are
  // the documented starting points.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "${runnable_tasks} runnable tasks on ${schedulers} schedulers, ${work_queue} queued; waits ms/s: cpu ${cpu_waits}, io ${io_waits}, "
                           "log ${log_waits}, lock ${lock_waits}, memory ${memory_waits}, signal ${signal_wait_pct}%",
                           "mssql", "%(status): No scheduler information returned", "");
  // The wait profile is a graphing source: emit everything without thresholds.
  filter_helper.set_default_perf_config(
      "extra(runnable_tasks;work_queue;workers;cpu_waits;io_waits;log_waits;lock_waits;latch_waits;memory_waits;network_waits;other_waits;total_waits;"
      "signal_wait_pct)");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result sched = session.execute(SCHEDULERS_SQL);
    const mssql_odbc::result res = session.execute(WAITS_SQL);
    std::vector<wait_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      wait_row row;
      row.wait_type = res.get_string(i, "wait_type");
      row.wait_ms = res.get_int(i, "wait_ms");
      row.signal_ms = res.get_int(i, "signal_ms");
      row.elapsed_ms = res.get_int(i, "elapsed_ms");
      rows.push_back(row);
    }

    waits_info summary = build_waits(rows);
    if (!sched.rows.empty()) {
      summary.schedulers = sched.get_int(0, "schedulers");
      summary.runnable_tasks = sched.get_int(0, "runnable_tasks");
      summary.work_queue = sched.get_int(0, "work_queue");
      summary.workers = sched.get_int(0, "workers");
    }

    if (!sched.rows.empty() || !rows.empty()) {
      auto record = std::make_shared<filter_obj>(summary);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_waits_command
