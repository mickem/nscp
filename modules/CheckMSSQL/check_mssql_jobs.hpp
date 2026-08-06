// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "odbc_query.hpp"

namespace check_mssql_jobs_command {

// Raw per-job row from msdb.dbo.sysjobs + latest sysjobhistory outcome.
struct job_row {
  std::string name;
  bool enabled = false;
  long long run_status = -1;    // sysjobhistory.run_status; -1 = never ran
  long long last_run_age = -1;  // seconds; -1 = never ran
};

// msdb run_status codes: 0=failed 1=succeeded 2=retry 3=canceled 4=running;
// -1 is our "never ran" marker.
std::string run_status_to_string(long long status);

// One SQL Agent job as exposed to the filter engine.
struct job_info {
  std::string name;
  bool enabled = false;
  long long last_run_outcome = -1;
  std::string last_run_status;  // failed / succeeded / retry / canceled / running / never / unknown
  long long last_run_age = -1;  // seconds; -1 = never ran

  std::string get_name() const { return name; }
  long long get_enabled() const { return enabled ? 1 : 0; }
  std::string get_last_run_status() const { return last_run_status; }
  long long get_last_run_outcome() const { return last_run_outcome; }
  long long get_last_run_age() const { return last_run_age; }

  std::string show() const { return name; }
};

typedef std::vector<job_info> jobs_type;

// Pure: normalize raw rows into filterable jobs.
jobs_type build_jobs(const std::vector<job_row> &rows);

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_mssql_jobs_command
