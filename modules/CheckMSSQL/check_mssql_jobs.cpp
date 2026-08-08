// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_jobs.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace check_mssql_jobs_command {

namespace {

// step_id = 0 is the job outcome row (per-step rows use step_id >= 1), written
// only once a run completes.
//
// last_run_age is measured from when the run *finished*: agent_datetime gives
// the start, and run_duration is an HHMMSS-encoded integer (13005 = 1h30m05s),
// so the elapsed run time has to be decoded and added. Measuring from the start
// would skew every threshold by the job's own duration.
//
// sysjobactivity supplies the in-flight state that sysjobhistory cannot: a job
// currently executing has no step-0 row yet, so without this a running job
// looks like it never ran. The latest session_id is the current Agent session.
const char *JOBS_SQL =
    "SELECT j.name, j.enabled,"
    " ISNULL(h.run_status, -1) AS run_status,"
    " ISNULL(DATEDIFF(second, DATEADD(second,"
    " (h.run_duration / 10000) * 3600 + ((h.run_duration / 100) % 100) * 60 + (h.run_duration % 100),"
    " msdb.dbo.agent_datetime(h.run_date, h.run_time)), GETDATE()), -1) AS last_run_age,"
    " CASE WHEN a.start_execution_date IS NOT NULL AND a.stop_execution_date IS NULL THEN 1 ELSE 0 END AS is_running"
    " FROM msdb.dbo.sysjobs j"
    " OUTER APPLY (SELECT TOP 1 run_status, run_date, run_time, run_duration FROM msdb.dbo.sysjobhistory h"
    " WHERE h.job_id = j.job_id AND h.step_id = 0 ORDER BY h.instance_id DESC) h"
    " OUTER APPLY (SELECT TOP 1 start_execution_date, stop_execution_date FROM msdb.dbo.sysjobactivity a"
    " WHERE a.job_id = j.job_id ORDER BY a.session_id DESC) a";

typedef job_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Job name")
      .add_string_var("last_run_status", &filter_obj::get_last_run_status, "Outcome of the last completed run: failed, succeeded, retry, canceled or never");

  static const parsers::where::value_type type_age = parsers::where::type_custom_int_1;
  registry_.add_converter(type_age, &mssql_filter::parse_time<std::shared_ptr<filter_obj>>);
  registry_
      .add_int_var("last_run_age", type_age, &filter_obj::get_last_run_age,
                   "Seconds since the last run finished, -1 = never ran (supports units, e.g. last_run_age > 25h)")
      .add_int_perf("s", "", "_last_run_age")
      .add_int_var("enabled", &filter_obj::get_enabled, "1 if the job is enabled")
      .no_perf()
      .add_int_var("is_running", &filter_obj::get_is_running, "1 if the job is executing right now")
      .no_perf()
      .add_int_var("last_run_outcome", &filter_obj::get_last_run_outcome, "Raw msdb run_status code of the last completed run (-1 = never ran)")
      .no_perf();
}

}  // namespace

std::string run_status_to_string(long long status) {
  switch (status) {
    case 0:
      return "failed";
    case 1:
      return "succeeded";
    case 2:
      return "retry";
    case 3:
      return "canceled";
    case 4:
      return "running";
    case -1:
      return "never";
    default:
      return "unknown";
  }
}

jobs_type build_jobs(const std::vector<job_row> &rows) {
  jobs_type result;
  for (const job_row &row : rows) {
    job_info info;
    info.name = row.name;
    info.enabled = row.enabled;
    info.is_running = row.is_running;
    info.last_run_outcome = row.run_status;
    info.last_run_status = run_status_to_string(row.run_status);
    info.last_run_age = row.last_run_age;
    result.push_back(info);
  }
  return result;
}

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;

  filter_type filter;
  // Only enabled jobs are considered by default. empty_state = "ok": no Agent
  // jobs (e.g. Express edition) is not a problem.
  filter_helper.add_options("last_run_status = 'canceled' or last_run_status = 'retry'", "last_run_status = 'failed'", "enabled = 1",
                            filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} jobs (${problem_list})", "${name}: ${last_run_status}", "${name}",
                           "%(status): No enabled SQL Agent jobs found", "%(status): All %(count) jobs succeeded");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(JOBS_SQL);
    std::vector<job_row> rows;
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      job_row row;
      row.name = res.get_string(i, "name");
      row.enabled = res.get_int(i, "enabled") != 0;
      row.run_status = res.get_int(i, "run_status");
      row.last_run_age = res.get_int(i, "last_run_age");
      row.is_running = res.get_int(i, "is_running") != 0;
      rows.push_back(row);
    }

    for (const job_info &job : build_jobs(rows)) {
      auto record = std::make_shared<filter_obj>(job);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_jobs_command
