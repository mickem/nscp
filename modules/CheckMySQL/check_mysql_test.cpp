// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_mysql.hpp"
#include "check_mysql_query.hpp"
#include "mysql_client.hpp"

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
static nscapi::helper_singleton test_plugin_singleton;
nscapi::helper_singleton *nscapi::plugin_singleton = &test_plugin_singleton;

using mysql_client::mysql_exception;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

mysql_client::result make_result(const std::vector<std::string> &columns, const std::vector<std::vector<std::string>> &rows) {
  mysql_client::result res;
  res.columns = columns;
  for (const auto &row : rows) {
    std::vector<mysql_client::cell> cells;
    for (const auto &text : row) {
      mysql_client::cell c;
      c.text = text;
      cells.push_back(c);
    }
    res.rows.push_back(cells);
  }
  return res;
}

// A healthy MariaDB server: version/comment/max_connections plus the two
// GLOBAL STATUS rows check_mysql asks for.
mysql_client::session_factory healthy_mariadb() {
  return [](const mysql_client::connection_info &) -> mysql_client::query_runner {
    return [](const std::string &sql) -> mysql_client::result {
      if (sql.find("@@version") != std::string::npos)
        return make_result({"version", "version_comment", "max_connections"}, {{"10.11.14-MariaDB-ubu2404", "Ubuntu 24.04", "151"}});
      if (sql.find("GLOBAL STATUS") != std::string::npos)
        return make_result({"Variable_name", "Value"}, {{"Uptime", "86400"}, {"Threads_connected", "3"}});
      throw mysql_exception("unexpected query: " + sql);
    };
  };
}

mysql_client::session_factory refusing_server(const std::string &error) {
  return [error](const mysql_client::connection_info &) -> mysql_client::query_runner { throw mysql_exception(error); };
}

mysql_client::session_factory failing_queries(const std::string &error) {
  return [error](const mysql_client::connection_info &) -> mysql_client::query_runner {
    return [error](const std::string &) -> mysql_client::result { throw mysql_exception(error); };
  };
}

PB::Common::ResultCode run_health(const mysql_client::session_factory &factory, const std::vector<std::string> &args,
                                  PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_mysql");
  for (const std::string &a : args) request.add_arguments(a);
  check_mysql_command::check_with(mysql_client::connection_info(), request, &response, factory);
  return response.result();
}

PB::Common::ResultCode run_query(const mysql_client::session_factory &factory, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_mysql_query");
  for (const std::string &a : args) request.add_arguments(a);
  check_mysql_query_command::check_with(mysql_client::connection_info(), request, &response, factory);
  return response.result();
}

}  // namespace

// --- flavor derivation -------------------------------------------------------

TEST(DeriveFlavor, ClassifiesTheKnownFamilies) {
  EXPECT_EQ(mysql_client::derive_flavor("10.11.14-MariaDB-ubu2404", "Ubuntu 24.04"), "mariadb");
  EXPECT_EQ(mysql_client::derive_flavor("8.4.3", "MySQL Community Server - GPL"), "mysql");
  EXPECT_EQ(mysql_client::derive_flavor("8.0.36-28", "Percona Server (GPL), Release 28"), "percona");
  // Flavor hidden in the comment only (e.g. a distro build of MariaDB).
  EXPECT_EQ(mysql_client::derive_flavor("10.6.1", "mariadb.org binary distribution"), "mariadb");
}

// --- mysql_client::result ----------------------------------------------------

TEST(Result, GetIntParsesIntegersAndRoundsDecimals) {
  const mysql_client::result res = make_result({"a", "b", "c"}, {{"42", "99.6", "-99.6"}});
  EXPECT_EQ(res.get_int(0, "a"), 42);
  EXPECT_EQ(res.get_int(0, "b"), 100);
  EXPECT_EQ(res.get_int(0, "c"), -100);
}

TEST(Result, MissingColumnThrows) {
  const mysql_client::result res = make_result({"a"}, {{"1"}});
  EXPECT_THROW(res.get_string(0, "nope"), mysql_exception);
}

// --- check_mysql -------------------------------------------------------------

TEST(CheckMysql, HealthyServerIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_health(healthy_mariadb(), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("mariadb 10.11.14-MariaDB-ubu2404"), std::string::npos) << msg;
  EXPECT_NE(msg.find("uptime 86400s"), std::string::npos) << msg;
  EXPECT_NE(msg.find("connections 3/151"), std::string::npos) << msg;
}

// A factory that records whether it was ever asked to connect, and with what.
// The security fix hangs on connect never being reached for a rejected option -
// an "UNKNOWN" that still dialed out would have loaded the plugin already.
struct connect_probe {
  bool called = false;
  mysql_client::connection_info seen;
  mysql_client::session_factory factory() {
    return [this](const mysql_client::connection_info &info) -> mysql_client::query_runner {
      called = true;
      seen = info;
      return [](const std::string &sql) -> mysql_client::result {
        if (sql.find("@@version") != std::string::npos)
          return make_result({"version", "version_comment", "max_connections"}, {{"8.4.3", "MySQL", "151"}});
        return make_result({"Variable_name", "Value"}, {{"Uptime", "1"}, {"Threads_connected", "1"}});
      };
    };
  }
};

// plugin-dir / socket / defaults-file are settings-only: as per-request options
// they are an arbitrary-DLL-load (plugin-dir), SMB-relay (socket) or arbitrary
// file read (defaults-file) as the service account. They must be rejected
// outright - not merely ignored - and the connect must never be attempted.
TEST(CheckMysql, CodeLoadingOptionsAreRejectedFromTheRequest) {
  for (const std::string &arg : {std::string("plugin-dir=\\\\attacker\\share"), std::string("socket=\\\\attacker\\pipe\\x"),
                                 std::string("defaults-file=\\\\attacker\\share\\my.cnf")}) {
    connect_probe probe;
    PB::Commands::QueryResponseMessage::Response response;
    EXPECT_EQ(run_health(probe.factory(), {arg}, response), PB::Common::ResultCode::UNKNOWN) << arg << ": " << join_lines(response);
    EXPECT_FALSE(probe.called) << arg << ": the connect was attempted despite the option being refused";
    EXPECT_NE(join_lines(response).find("Invalid command line"), std::string::npos) << arg << ": " << join_lines(response);
  }
}

// The same parameters are still honoured from /settings/mysql (the defaults the
// module hands to check_with), so the legitimate deployment case keeps working -
// this is a move to config, not a removal.
TEST(CheckMysql, PluginDirFromSettingsStillReachesTheConnection) {
  mysql_client::connection_info defaults;
  defaults.plugin_dir = "/opt/mysql/plugin";
  defaults.socket = "/var/run/mysqld/mysqld.sock";

  connect_probe probe;
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_mysql");
  PB::Commands::QueryResponseMessage::Response response;
  check_mysql_command::check_with(defaults, request, &response, probe.factory());

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  ASSERT_TRUE(probe.called);
  EXPECT_EQ(probe.seen.plugin_dir, "/opt/mysql/plugin");
  EXPECT_EQ(probe.seen.socket, "/var/run/mysqld/mysqld.sock");
}

TEST(CheckMysql, UptimeThresholdSupportsTimeUnits) {
  // Uptime is 86400s = 1d, so warning on "< 2d" must trip.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_health(healthy_mariadb(), {"warning=uptime < 2d"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckMysql, ConnectionThresholdsWork) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_health(healthy_mariadb(), {"critical=threads_connected > 2"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST(CheckMysql, ConnectFailureIsUnknownWithTarget) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_health(refusing_server("Access denied for user 'x'"), {}, response), PB::Common::ResultCode::UNKNOWN);
  const std::string msg = join_lines(response);
  EXPECT_NE(msg.find("Failed to connect to MySQL server 'localhost:3306'"), std::string::npos) << msg;
  EXPECT_NE(msg.find("Access denied"), std::string::npos) << msg;
}

TEST(CheckMysql, QueryFailureIsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_health(failing_queries("server has gone away"), {}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Query failed: server has gone away"), std::string::npos) << join_lines(response);
}

// --- check_mysql_query -------------------------------------------------------

TEST(CheckMysqlQuery, NoQuerySpecifiedReturnsUnknown) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_query(healthy_mariadb(), {}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No query specified"), std::string::npos) << join_lines(response);
}

TEST(CheckMysqlQuery, ColumnsBecomeKeywordsAndThresholdsApply) {
  const auto factory = [](const mysql_client::connection_info &) -> mysql_client::query_runner {
    return [](const std::string &) -> mysql_client::result {
      return make_result({"name", "value"}, {{"queue_a", "3"}, {"queue_b", "12"}});
    };
  };
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_mysql_query");
  request.add_arguments("query=SELECT name, value FROM queues");
  request.add_arguments("warning=value > 10");
  request.add_arguments("detail-syntax=%(name)=%(value)");
  request.add_arguments("top-syntax=${status}: ${problem_list}");
  check_mysql_query_command::check_with(mysql_client::connection_info(), request, &response, factory);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("queue_b=12"), std::string::npos) << join_lines(response);
}

TEST(CheckMysqlQuery, StatementWithoutResultSetIsUnknown) {
  const auto factory = [](const mysql_client::connection_info &) -> mysql_client::query_runner {
    return [](const std::string &) -> mysql_client::result { return mysql_client::result(); };
  };
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_query(factory, {"query=SET @x = 1"}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("no result set"), std::string::npos) << join_lines(response);
}
