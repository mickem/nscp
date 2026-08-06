// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_mssql_backup.hpp"
#include "check_mssql_databases.hpp"
#include "check_mssql_jobs.hpp"
#include "check_mssql_query.hpp"
#include "mssql_filter_helpers.hpp"
#include "odbc_query.hpp"

// Normally provided by NSC_WRAP_DLL() in the auto-generated module.cpp; in the
// test binary there is no generated module, so define the singleton here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

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
