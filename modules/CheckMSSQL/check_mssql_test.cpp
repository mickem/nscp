// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_mssql_availability_groups.hpp"
#include "check_mssql_backup.hpp"
#include "check_mssql_blocking.hpp"
#include "check_mssql_counters.hpp"
#include "check_mssql_databases.hpp"
#include "check_mssql_integrity.hpp"
#include "check_mssql_jobs.hpp"
#include "check_mssql_query.hpp"
#include "check_mssql_sessions.hpp"
#include "check_mssql_tempdb.hpp"
#include "check_mssql_transactions.hpp"
#include "check_mssql_waits.hpp"
#include "mssql_filter_helpers.hpp"
#include "odbc_query.hpp"

// Normally provided by NSC_WRAP_DLL() in the auto-generated module.cpp; in the
// test binary there is no generated module, so define the singleton here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &response) {
  std::string out;
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += response.lines(i).message();
  }
  return out;
}

}  // namespace

// --- connection string builder -------------------------------------------------

TEST(ConnectionString, TrustedConnectionByDefault) {
  mssql_odbc::connection_info info;
  const std::string cs = info.to_connection_string("ODBC Driver 18 for SQL Server");
  EXPECT_EQ(cs, "Driver={ODBC Driver 18 for SQL Server};Server=localhost;Trusted_Connection=yes;TrustServerCertificate=yes;APP=NSClient++;");
}

TEST(ConnectionString, SqlAuthWhenUserGiven) {
  mssql_odbc::connection_info info;
  info.server = "db1\\PROD";
  info.user = "monitor";
  info.password = "secret";
  const std::string cs = info.to_connection_string("ODBC Driver 17 for SQL Server");
  EXPECT_NE(cs.find("UID=monitor;PWD=secret;"), std::string::npos) << cs;
  EXPECT_EQ(cs.find("Trusted_Connection"), std::string::npos) << cs;
}

TEST(ConnectionString, PasswordWithSpecialCharactersIsBraceQuoted) {
  mssql_odbc::connection_info info;
  info.user = "monitor";
  info.password = "p;w}d";
  const std::string cs = info.to_connection_string("ODBC Driver 18 for SQL Server");
  EXPECT_NE(cs.find("PWD={p;w}}d};"), std::string::npos) << cs;
}

TEST(ConnectionString, LegacyDriverGetsNoEncryptKeywords) {
  mssql_odbc::connection_info info;
  info.encrypt = "no";
  const std::string cs = info.to_connection_string("SQL Server");
  EXPECT_EQ(cs.find("TrustServerCertificate"), std::string::npos) << cs;
  EXPECT_EQ(cs.find("Encrypt"), std::string::npos) << cs;
}

TEST(ConnectionString, ModernDriverHonorsEncryptAndTrustCert) {
  mssql_odbc::connection_info info;
  info.encrypt = "no";
  info.trust_server_cert = false;
  const std::string cs = info.to_connection_string("ODBC Driver 18 for SQL Server");
  EXPECT_NE(cs.find("Encrypt=no;"), std::string::npos) << cs;
  EXPECT_EQ(cs.find("TrustServerCertificate"), std::string::npos) << cs;
}

TEST(ConnectionString, RawConnectionStringWinsOverEverything) {
  mssql_odbc::connection_info info;
  info.user = "ignored";
  info.raw_connection_string = "DSN=mydsn;UID=x;PWD=y;";
  EXPECT_EQ(info.to_connection_string("ODBC Driver 18 for SQL Server"), "DSN=mydsn;UID=x;PWD=y;");
}

// --- driver selection ------------------------------------------------------------

TEST(PickDriver, PrefersNewestModernDriver) {
  const std::vector<std::string> installed = {"SQL Server", "ODBC Driver 17 for SQL Server", "ODBC Driver 18 for SQL Server", "PostgreSQL Unicode"};
  EXPECT_EQ(mssql_odbc::session::pick_driver(installed), "ODBC Driver 18 for SQL Server");
}

TEST(PickDriver, FallsBackToLegacyDriver) {
  const std::vector<std::string> installed = {"PostgreSQL Unicode", "SQL Server"};
  EXPECT_EQ(mssql_odbc::session::pick_driver(installed), "SQL Server");
}

TEST(PickDriver, DefaultsToLegacyNameWhenNothingMatches) { EXPECT_EQ(mssql_odbc::session::pick_driver({}), "SQL Server"); }

// --- pure builders ---------------------------------------------------------------

TEST(BuildDatabases, MergesLogspaceByNameAndDefaultsToUnknown) {
  std::vector<check_mssql_databases_command::database_row> databases(2);
  databases[0].name = "master";
  databases[0].state = "ONLINE";
  databases[0].recovery_model = "SIMPLE";
  databases[0].data_bytes = 8 * 1024 * 1024;
  databases[0].log_bytes = 2 * 1024 * 1024;
  databases[1].name = "orphan";
  databases[1].state = "OFFLINE";

  std::vector<check_mssql_databases_command::logspace_row> logspace(1);
  logspace[0].name = "master";
  logspace[0].used_pct = 42;

  const auto out = check_mssql_databases_command::build_databases(databases, logspace);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].log_used_pct, 42);
  EXPECT_EQ(out[0].data_size, 8 * 1024 * 1024);
  EXPECT_EQ(out[1].log_used_pct, -1);  // not present in LOGSPACE output
}

TEST(BuildBackups, NeverBackedUpMapsToMinusOne) {
  std::vector<check_mssql_backup_command::backup_row> rows(2);
  rows[0].name = "appdb";
  rows[0].recovery_model = "FULL";
  rows[0].has_full = true;
  rows[0].full_age = 3600;
  rows[0].has_log = true;
  rows[0].log_age = 120;
  rows[1].name = "newdb";  // never backed up: has_* all false

  const auto out = check_mssql_backup_command::build_backups(rows);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].full_age, 3600);
  EXPECT_EQ(out[0].diff_age, -1);
  EXPECT_EQ(out[0].log_age, 120);
  EXPECT_EQ(out[1].full_age, -1);
  EXPECT_EQ(out[1].log_age, -1);
}

TEST(RunStatus, MapsAllKnownCodes) {
  using check_mssql_jobs_command::run_status_to_string;
  EXPECT_EQ(run_status_to_string(0), "failed");
  EXPECT_EQ(run_status_to_string(1), "succeeded");
  EXPECT_EQ(run_status_to_string(2), "retry");
  EXPECT_EQ(run_status_to_string(3), "canceled");
  EXPECT_EQ(run_status_to_string(4), "running");
  EXPECT_EQ(run_status_to_string(-1), "never");
  EXPECT_EQ(run_status_to_string(99), "unknown");
}

TEST(BuildBackupSql, ExcludesCopyOnlyAndSnapshotInTheJoinNotTheWhere) {
  const std::string sql = check_mssql_backup_command::build_backup_sql(false, false);
  const std::size_t on_pos = sql.find("ON b.database_name = d.name");
  const std::size_t where_pos = sql.find("WHERE");
  ASSERT_NE(on_pos, std::string::npos) << sql;
  ASSERT_NE(where_pos, std::string::npos) << sql;
  const std::size_t copy_pos = sql.find("b.is_copy_only = 0");
  const std::size_t snap_pos = sql.find("b.is_snapshot = 0");
  ASSERT_NE(copy_pos, std::string::npos) << sql;
  ASSERT_NE(snap_pos, std::string::npos) << sql;
  // In WHERE these predicates would turn the LEFT JOIN into an inner join and
  // hide every never-backed-up database, which is the whole point of the check.
  EXPECT_GT(copy_pos, on_pos) << sql;
  EXPECT_LT(copy_pos, where_pos) << sql;
  EXPECT_GT(snap_pos, on_pos) << sql;
  EXPECT_LT(snap_pos, where_pos) << sql;
}

TEST(BuildBackupSql, IncludeFlagsDropThePredicates) {
  const std::string both = check_mssql_backup_command::build_backup_sql(true, true);
  EXPECT_EQ(both.find("is_copy_only"), std::string::npos) << both;
  EXPECT_EQ(both.find("is_snapshot"), std::string::npos) << both;

  const std::string copy_only = check_mssql_backup_command::build_backup_sql(true, false);
  EXPECT_EQ(copy_only.find("is_copy_only"), std::string::npos) << copy_only;
  EXPECT_NE(copy_only.find("b.is_snapshot = 0"), std::string::npos) << copy_only;
}

TEST(BuildJobs, RunningJobIsReportedSeparatelyFromLastOutcome) {
  std::vector<check_mssql_jobs_command::job_row> rows(1);
  rows[0].name = "etl";
  rows[0].enabled = true;
  rows[0].run_status = 1;  // last completed run succeeded
  rows[0].last_run_age = 120;
  rows[0].is_running = true;  // and it is executing again right now

  const auto out = check_mssql_jobs_command::build_jobs(rows);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].get_is_running(), 1);
  EXPECT_EQ(out[0].last_run_status, "succeeded");
}

TEST(BuildJobs, NeverRanJob) {
  std::vector<check_mssql_jobs_command::job_row> rows(1);
  rows[0].name = "nightly";
  rows[0].enabled = true;
  rows[0].run_status = -1;
  rows[0].last_run_age = -1;

  const auto out = check_mssql_jobs_command::build_jobs(rows);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].last_run_status, "never");
  EXPECT_EQ(out[0].last_run_age, -1);
}

TEST(BuildBlocking, ResolvesChainsToTheRootBlocker) {
  // 70 -> 60 -> 50, where 50 is not blocked: 50 is the root for both.
  std::vector<check_mssql_blocking_command::blocking_row> rows(2);
  rows[0].session_id = 60;
  rows[0].blocking_session_id = 50;
  rows[1].session_id = 70;
  rows[1].blocking_session_id = 60;

  const auto out = check_mssql_blocking_command::build_blocking(rows);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].root_blocker, 50);
  EXPECT_EQ(out[1].root_blocker, 50);
}

TEST(BuildBlocking, CycleTerminatesInsteadOfLooping) {
  // 60 -> 70 -> 60: a deadlock in flight must not hang the chain walk.
  std::vector<check_mssql_blocking_command::blocking_row> rows(2);
  rows[0].session_id = 60;
  rows[0].blocking_session_id = 70;
  rows[1].session_id = 70;
  rows[1].blocking_session_id = 60;

  const auto out = check_mssql_blocking_command::build_blocking(rows);
  ASSERT_EQ(out.size(), 2u);
  // The walk stops at the first revisited session; both roots stay in the cycle.
  EXPECT_TRUE(out[0].root_blocker == 60 || out[0].root_blocker == 70);
  EXPECT_TRUE(out[1].root_blocker == 60 || out[1].root_blocker == 70);
}

TEST(BuildBlocking, DirectBlockerFieldsPassThrough) {
  std::vector<check_mssql_blocking_command::blocking_row> rows(1);
  rows[0].session_id = 61;
  rows[0].blocking_session_id = 52;
  rows[0].database = "appdb";
  rows[0].login = "app";
  rows[0].blocking_login = "batch";
  rows[0].wait_time = 42;
  rows[0].wait_type = "LCK_M_X";
  rows[0].command = "UPDATE";
  rows[0].blocker_idle = true;

  const auto out = check_mssql_blocking_command::build_blocking(rows);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].root_blocker, 52);  // blocker not itself blocked
  EXPECT_EQ(out[0].get_blocker_idle(), 1);
  EXPECT_EQ(out[0].show(), "61");
}

TEST(BuildReplicas, DatabaseRowsUseDatabaseHealthAndComposeTheName) {
  std::vector<check_mssql_availability_groups_command::replica_row> rows(2);
  rows[0].group = "ag1";
  rows[0].replica = "sql1";
  rows[0].role = "PRIMARY";
  rows[0].replica_health = "HEALTHY";
  rows[0].database = "appdb";
  rows[0].db_health = "NOT_HEALTHY";
  rows[0].sync_state = "NOT SYNCHRONIZING";
  rows[1].group = "ag1";
  rows[1].replica = "sql2";
  rows[1].role = "SECONDARY";
  rows[1].replica_health = "PARTIALLY_HEALTHY";  // replica-level row, no database

  const auto out = check_mssql_availability_groups_command::build_replicas(rows);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].name, "ag1/sql1/appdb");
  EXPECT_EQ(out[0].health, "NOT_HEALTHY");  // database health wins on database rows
  EXPECT_EQ(out[1].name, "ag1/sql2");
  EXPECT_EQ(out[1].health, "PARTIALLY_HEALTHY");  // replica health on replica rows
}

namespace {

check_mssql_counters_command::counter_row make_counter(const std::string &name, long long value, long long prev, long long elapsed_ms = 1000) {
  check_mssql_counters_command::counter_row row;
  row.name = name;
  row.value = value;
  row.has_prev = true;
  row.prev_value = prev;
  row.elapsed_ms = elapsed_ms;
  return row;
}

}  // namespace

TEST(BuildCounters, RatesUseTheMeasuredWindowNotTheNominalSecond) {
  // 250 batches in a 2000ms window is 125/s, not 250/s.
  std::vector<check_mssql_counters_command::counter_row> rows;
  rows.push_back(make_counter("Batch Requests/sec", 10250, 10000, 2000));
  rows.push_back(make_counter("Page life expectancy", 4211, 4210));

  const auto out = check_mssql_counters_command::build_counters(rows);
  EXPECT_DOUBLE_EQ(out.batch_requests, 125.0);
  EXPECT_EQ(out.page_life_expectancy, 4211);  // point-in-time, no delta
}

TEST(BuildCounters, HitRatioIsComputedOverTheWindow) {
  // Lifetime ratio is ~99.99% but the window saw 90/100: report 90%.
  std::vector<check_mssql_counters_command::counter_row> rows;
  rows.push_back(make_counter("Buffer cache hit ratio", 1000090, 1000000));
  rows.push_back(make_counter("Buffer cache hit ratio base", 1000200, 1000100));

  const auto out = check_mssql_counters_command::build_counters(rows);
  EXPECT_DOUBLE_EQ(out.hit_ratio, 90.0);
}

TEST(BuildCounters, HitRatioFallsBackToLifetimeWhenTheBaseDidNotMove) {
  std::vector<check_mssql_counters_command::counter_row> rows;
  rows.push_back(make_counter("Buffer cache hit ratio", 999, 999));
  rows.push_back(make_counter("Buffer cache hit ratio base", 1000, 1000));

  const auto out = check_mssql_counters_command::build_counters(rows);
  EXPECT_DOUBLE_EQ(out.hit_ratio, 99.9);
}

TEST(BuildCounters, MissingCountersReportMinusOne) {
  const auto out = check_mssql_counters_command::build_counters({});
  EXPECT_DOUBLE_EQ(out.hit_ratio, -1);
  EXPECT_EQ(out.page_life_expectancy, -1);
  EXPECT_DOUBLE_EQ(out.batch_requests, -1);
  EXPECT_DOUBLE_EQ(out.deadlocks, -1);
  EXPECT_DOUBLE_EQ(out.lock_waits, -1);
}

TEST(BuildCounters, MissingFirstSnapshotYieldsMinusOneRate) {
  // A counter present only in the second snapshot has no delta to rate.
  check_mssql_counters_command::counter_row row;
  row.name = "SQL Compilations/sec";
  row.value = 500;
  row.has_prev = false;
  row.elapsed_ms = 1000;

  const auto out = check_mssql_counters_command::build_counters({row});
  EXPECT_DOUBLE_EQ(out.compilations, -1);
}

TEST(BuildSessions, UnknownIdleAgeMapsToMinusOne) {
  std::vector<check_mssql_sessions_command::session_row> rows(2);
  rows[0].database = "appdb";
  rows[0].login = "app";
  rows[0].sessions = 12;
  rows[0].running = 3;
  rows[0].idle = 9;
  rows[0].connections = 12;
  rows[0].has_max_idle = true;
  rows[0].max_idle = 3600;
  rows[1].database = "master";
  rows[1].login = "monitor";  // just connected: no completed request yet
  rows[1].sessions = 1;

  const auto out = check_mssql_sessions_command::build_sessions(rows);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].max_idle, 3600);
  EXPECT_EQ(out[0].show(), "appdb/app");
  EXPECT_EQ(out[1].max_idle, -1);  // unknown-idle contract
  EXPECT_EQ(out[1].show(), "master/monitor");
}

TEST(BuildSessions, MissingDatabaseShowsLoginOnly) {
  std::vector<check_mssql_sessions_command::session_row> rows(1);
  rows[0].login = "app";
  const auto out = check_mssql_sessions_command::build_sessions(rows);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].show(), "app");
}

TEST(CategorizeWait, MapsTheDiagnosticCategories) {
  using check_mssql_waits_command::categorize_wait;
  EXPECT_EQ(categorize_wait("SOS_SCHEDULER_YIELD"), "cpu");
  EXPECT_EQ(categorize_wait("THREADPOOL"), "cpu");
  EXPECT_EQ(categorize_wait("CXPACKET"), "cpu");
  EXPECT_EQ(categorize_wait("PAGEIOLATCH_SH"), "io");
  EXPECT_EQ(categorize_wait("WRITELOG"), "log");
  EXPECT_EQ(categorize_wait("LCK_M_X"), "lock");
  EXPECT_EQ(categorize_wait("PAGELATCH_EX"), "latch");
  EXPECT_EQ(categorize_wait("RESOURCE_SEMAPHORE"), "memory");
  EXPECT_EQ(categorize_wait("ASYNC_NETWORK_IO"), "network");
  EXPECT_EQ(categorize_wait("SOME_FUTURE_WAIT"), "other");
}

TEST(CategorizeWait, IdleHousekeepingWaitsAreBenign) {
  using check_mssql_waits_command::categorize_wait;
  EXPECT_EQ(categorize_wait("LAZYWRITER_SLEEP"), "benign");
  EXPECT_EQ(categorize_wait("SOS_WORK_DISPATCHER"), "benign");
  EXPECT_EQ(categorize_wait("HADR_TIMER_TASK"), "benign");
  EXPECT_EQ(categorize_wait("XE_TIMER_EVENT"), "benign");
  EXPECT_EQ(categorize_wait("WAITFOR"), "benign");  // includes this check's own sampling delay
  EXPECT_EQ(categorize_wait("CHECKPOINT_QUEUE"), "benign");
}

namespace {

check_mssql_waits_command::wait_row make_wait(const std::string &type, long long wait_ms, long long signal_ms, long long elapsed_ms = 1000) {
  check_mssql_waits_command::wait_row row;
  row.wait_type = type;
  row.wait_ms = wait_ms;
  row.signal_ms = signal_ms;
  row.elapsed_ms = elapsed_ms;
  return row;
}

}  // namespace

TEST(BuildWaits, RatesAndSignalPctExcludeBenignWaits) {
  std::vector<check_mssql_waits_command::wait_row> rows;
  rows.push_back(make_wait("PAGEIOLATCH_SH", 500, 50, 2000));  // io: 250 ms/s over a 2s window
  rows.push_back(make_wait("LCK_M_X", 300, 30, 2000));         // lock: 150 ms/s
  rows.push_back(make_wait("LAZYWRITER_SLEEP", 100000, 0, 2000));  // benign: excluded everywhere

  const auto out = check_mssql_waits_command::build_waits(rows);
  EXPECT_DOUBLE_EQ(out.io_waits, 250.0);
  EXPECT_DOUBLE_EQ(out.lock_waits, 150.0);
  EXPECT_DOUBLE_EQ(out.total_waits, 400.0);
  EXPECT_DOUBLE_EQ(out.other_waits, 0.0);
  EXPECT_DOUBLE_EQ(out.signal_wait_pct, 10.0);  // (50 + 30) / (500 + 300)
}

TEST(BuildWaits, QuietWindowReportsMinusOneSignalPct) {
  const auto out = check_mssql_waits_command::build_waits({});
  EXPECT_DOUBLE_EQ(out.signal_wait_pct, -1);
  EXPECT_DOUBLE_EQ(out.total_waits, 0.0);
}

TEST(ParseSqlDatetime, ParsesAndDiffsWithoutTimezone) {
  using check_mssql_integrity_command::parse_sql_datetime;
  const long long a = parse_sql_datetime("2026-08-12 10:00:00.000");
  const long long b = parse_sql_datetime("2026-08-11 10:00:00.000");
  ASSERT_GT(a, 0);
  EXPECT_EQ(a - b, 86400);
  EXPECT_EQ(parse_sql_datetime("not a date"), -1);
  EXPECT_EQ(parse_sql_datetime(""), -1);
  EXPECT_EQ(parse_sql_datetime("2026-13-40 10:00:00"), -1);
}

TEST(BuildIntegrity, MapsNeverUnknownAndAge) {
  std::vector<check_mssql_integrity_command::integrity_row> rows(3);
  rows[0].name = "appdb";
  rows[0].last_checkdb = "2026-08-10 03:00:00.000";
  rows[0].suspect_pages = 2;
  rows[1].name = "newdb";
  rows[1].last_checkdb = "1900-01-01 00:00:00.000";  // never checked
  rows[2].name = "nodbcc";
  rows[2].last_checkdb = "";  // DBCC DBINFO unavailable

  const auto out = check_mssql_integrity_command::build_integrity(rows, "2026-08-12 03:00:00.000");
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[0].checkdb_age, 2 * 86400);
  EXPECT_EQ(out[0].suspect_pages, 2);
  EXPECT_EQ(out[1].checkdb_age, -1);
  EXPECT_EQ(out[2].checkdb_age, -2);
}

TEST(BuildTempdb, DerivesUsedAndPercent) {
  check_mssql_tempdb_command::tempdb_row row;
  row.size = 1000 * 8192;
  row.free = 250 * 8192;
  row.version_store = 100 * 8192;
  row.user_objects = 400 * 8192;
  row.internal_objects = 200 * 8192;
  row.volume_free = 5LL * 1024 * 1024 * 1024;

  const auto out = check_mssql_tempdb_command::build_tempdb(row);
  EXPECT_EQ(out.used, 750 * 8192);
  EXPECT_EQ(out.used_pct, 75);
  EXPECT_EQ(out.volume_free, 5LL * 1024 * 1024 * 1024);
}

TEST(BuildTempdb, EmptyTempdbDoesNotDivideByZero) {
  const auto out = check_mssql_tempdb_command::build_tempdb({});
  EXPECT_EQ(out.used_pct, 0);
  EXPECT_EQ(out.volume_free, -1);  // default: unknown
}

// --- result helpers --------------------------------------------------------------

TEST(Result, GetIntParsesIntegersAndRoundsDecimals) {
  mssql_odbc::result res;
  res.columns = {"a", "b", "c", "d"};
  res.rows.push_back({{"123", false}, {"99.6", false}, {"", true}, {"-99.6", false}});
  EXPECT_EQ(res.get_int(0, "a"), 123);
  EXPECT_EQ(res.get_int(0, "b"), 100);
  EXPECT_EQ(res.get_int(0, "c"), 0);
  // Rounds to nearest in both directions: +0.5 truncation would give -99 here.
  EXPECT_EQ(res.get_int(0, "d"), -100);
  EXPECT_TRUE(res.is_null(0, "c"));
  EXPECT_THROW(res.find_column("missing"), mssql_odbc::odbc_exception);
}

// --- duration-literal converter ---------------------------------------------------

// parse_time must let plain integers (notably the negative "-1 = never"
// sentinel used by full_age / diff_age / log_age / last_run_age) bypass
// stox_as_time_sec, which rejects signs and would otherwise make the converter
// yield 0 - turning `full_age = -1` into `full_age = 0`, which never fires.
TEST(ParseTime, PlainIntegersIncludingNegativesAreRecognized) {
  EXPECT_TRUE(mssql_filter::is_plain_integer("0"));
  EXPECT_TRUE(mssql_filter::is_plain_integer("-1"));
  EXPECT_TRUE(mssql_filter::is_plain_integer("-2"));
  EXPECT_TRUE(mssql_filter::is_plain_integer("+259200"));
  EXPECT_TRUE(mssql_filter::is_plain_integer("259200"));
}

TEST(ParseTime, DurationSpecsAreNotTreatedAsPlainIntegers) {
  EXPECT_FALSE(mssql_filter::is_plain_integer("7d"));
  EXPECT_FALSE(mssql_filter::is_plain_integer("30m"));
  EXPECT_FALSE(mssql_filter::is_plain_integer("1h"));
  EXPECT_FALSE(mssql_filter::is_plain_integer("-"));
  EXPECT_FALSE(mssql_filter::is_plain_integer(""));
  EXPECT_FALSE(mssql_filter::is_plain_integer("abc"));
}

// --- command error contracts (no SQL Server needed) -------------------------------

TEST(CheckMssqlQuery, NoQuerySpecifiedReturnsUnknown) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_mssql_query");

  const mssql_odbc::connection_info defaults;
  check_mssql_query_command::check(defaults, request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No query specified"), std::string::npos) << join_lines(response);
}

#ifdef WIN32
// The default thresholds mix or/and ("transaction_age > 1800 or is_idle = 1
// and transaction_age > 300"): guard that the expression builds. A precedence
// regression would surface here as an UNKNOWN parse error instead of the
// connect-failure contract.
TEST(CheckMssqlTransactions, DefaultThresholdExpressionParses) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_mssql_transactions");
  request.add_arguments("server=localhost");
  request.add_arguments("driver=No Such Driver 99");

  const mssql_odbc::connection_info defaults;
  check_mssql_transactions_command::check(defaults, request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Failed to connect to SQL Server 'localhost'"), std::string::npos) << join_lines(response);
}

// The bogus driver makes SQLDriverConnectW fail immediately (IM002) with no
// network involved, pinning the connect-failure contract on every machine.
TEST(CheckMssql, ConnectFailureReturnsUnknownWithContractMessage) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_mssql");
  request.add_arguments("server=localhost");
  request.add_arguments("driver=No Such Driver 99");

  const mssql_odbc::connection_info defaults;
  check_mssql_command::check(defaults, request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Failed to connect to SQL Server 'localhost'"), std::string::npos) << join_lines(response);
}
#endif
