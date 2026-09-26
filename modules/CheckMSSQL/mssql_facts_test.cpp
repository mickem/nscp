// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mssql_facts.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>
#include <string>
#include <vector>

// The plugin singleton is defined in check_mssql_test.cpp, which is compiled
// into the same test binary.

namespace {

using nscapi::facts::testing::find_set;
using nscapi::facts::testing::json_of;

std::string mssql_json(const nscapi::facts::response &out) { return json_of(out, "mssql"); }

// "<null>" is a NULL cell, as ODBC hands back a SERVERPROPERTY the server
// does not know.
mssql_odbc::result make_result(const std::vector<std::string> &columns, const std::vector<std::vector<std::string>> &rows) {
  mssql_odbc::result res;
  res.columns = columns;
  for (const std::vector<std::string> &row : rows) {
    std::vector<mssql_odbc::cell> cells;
    for (const std::string &text : row) {
      mssql_odbc::cell c;
      c.null = text == "<null>";
      c.text = c.null ? "" : text;
      cells.push_back(c);
    }
    res.rows.push_back(cells);
  }
  return res;
}

const std::vector<std::string> SERVER_COLUMNS = {"server_name",   "machine_name",         "instance_name",   "version",
                                                 "product_level", "product_update_level", "edition",         "engine_edition",
                                                 "collation",     "is_clustered",         "is_hadr_enabled", "is_integrated_security_only"};
const std::vector<std::string> DATABASE_COLUMNS = {"name", "recovery_model", "collation", "compatibility_level", "create_date", "is_read_only"};

// A named instance of SQL Server 2022 Standard, with the four system
// databases and one of the operator's - in the storage order a server
// without the ORDER BY might hand back.
struct fake_server {
  std::vector<std::string> statements;

  mssql_facts::query_runner runner() {
    return [this](const std::string &sql) -> mssql_odbc::result {
      statements.push_back(sql);
      if (sql.find("SERVERPROPERTY") != std::string::npos) {
        return make_result(SERVER_COLUMNS, {{"DB01\\PROD", "DB01", "PROD", "16.0.4135.4", "RTM", "CU15", "Standard Edition (64-bit)", "2",
                                             "SQL_Latin1_General_CP1_CI_AS", "0", "1", "0"}});
      }
      if (sql.find("sys.databases") != std::string::npos) {
        return make_result(DATABASE_COLUMNS, {{"shop", "FULL", "Latin1_General_100_CI_AS_SC_UTF8", "160", "2026-03-02", "0"},
                                              {"master", "SIMPLE", "SQL_Latin1_General_CP1_CI_AS", "160", "2003-04-08", "0"},
                                              {"model", "FULL", "SQL_Latin1_General_CP1_CI_AS", "160", "2003-04-08", "0"},
                                              {"msdb", "SIMPLE", "SQL_Latin1_General_CP1_CI_AS", "160", "2022-10-08", "0"},
                                              {"tempdb", "SIMPLE", "SQL_Latin1_General_CP1_CI_AS", "160", "2026-09-25", "0"}});
      }
      throw mssql_odbc::odbc_exception("unexpected query: " + sql);
    };
  }
};

mssql_facts::selection both() {
  mssql_facts::selection what;
  what.server = what.databases = true;
  return what;
}

}  // namespace

TEST(MssqlFacts, ServerRecordCarriesTheInstanceNotItsUptime) {
  fake_server server;
  mssql_facts::selection what;
  what.server = true;
  const mssql_facts::snapshot snap = mssql_facts::gather(what, server.runner());
  EXPECT_EQ(server.statements.size(), 1u);

  nscapi::facts::response out;
  mssql_facts::publish(what, snap, 0, out);
  // No uptime: it changes every round and belongs to check_mssql. And nothing
  // from [/settings/mssql]: the names are what the server says about itself.
  EXPECT_EQ(mssql_json(out),
            "{\"server_name\":\"DB01\\\\PROD\",\"machine_name\":\"DB01\",\"instance_name\":\"PROD\",\"version\":\"16.0.4135.4\",\"product_level\":\"RTM\","
            "\"product_update_level\":\"CU15\",\"edition\":\"Standard Edition (64-bit)\",\"engine_edition\":\"standard\","
            "\"collation\":\"SQL_Latin1_General_CP1_CI_AS\",\"authentication\":\"mixed\",\"clustered\":false,\"always_on\":true}");
}

TEST(MssqlFacts, EngineEditionIsNamedAndAnUnknownOneKeepsItsNumber) {
  EXPECT_EQ(mssql_facts::engine_edition_name(3), "enterprise");
  EXPECT_EQ(mssql_facts::engine_edition_name(4), "express");
  EXPECT_EQ(mssql_facts::engine_edition_name(5), "azure_sql_database");
  EXPECT_EQ(mssql_facts::engine_edition_name(8), "azure_sql_managed_instance");
  EXPECT_EQ(mssql_facts::engine_edition_name(42), "42");
  EXPECT_EQ(mssql_facts::engine_edition_name(0), "");
}

TEST(MssqlFacts, DatabasesAreRecordsByNameSorted) {
  fake_server server;
  mssql_facts::selection what;
  what.databases = true;
  const mssql_facts::snapshot snap = mssql_facts::gather(what, server.runner());
  EXPECT_EQ(server.statements.size(), 1u) << "only the list was asked for";
  ASSERT_EQ(snap.databases.size(), 5u);
  EXPECT_EQ(snap.databases[0].id, "master");
  EXPECT_EQ(snap.databases[4].id, "tempdb");

  nscapi::facts::response out;
  mssql_facts::publish(what, snap, 0, out);
  // No state, no data or log size: those change every round and belong to
  // check_mssql_databases.
  EXPECT_EQ(mssql_json(out),
            "{\"databases\":["
            "{\"id\":\"master\",\"recovery_model\":\"SIMPLE\",\"collation\":\"SQL_Latin1_General_CP1_CI_AS\",\"compatibility_level\":160,\"create_date\":"
            "\"2003-04-08\",\"read_only\":false},"
            "{\"id\":\"model\",\"recovery_model\":\"FULL\",\"collation\":\"SQL_Latin1_General_CP1_CI_AS\",\"compatibility_level\":160,\"create_date\":\"2003-"
            "04-08\",\"read_only\":false},"
            "{\"id\":\"msdb\",\"recovery_model\":\"SIMPLE\",\"collation\":\"SQL_Latin1_General_CP1_CI_AS\",\"compatibility_level\":160,\"create_date\":\"2022-"
            "10-08\",\"read_only\":false},"
            "{\"id\":\"shop\",\"recovery_model\":\"FULL\",\"collation\":\"Latin1_General_100_CI_AS_SC_UTF8\",\"compatibility_level\":160,\"create_date\":"
            "\"2026-03-02\",\"read_only\":false},"
            "{\"id\":\"tempdb\",\"recovery_model\":\"SIMPLE\",\"collation\":\"SQL_Latin1_General_CP1_CI_AS\",\"compatibility_level\":160,\"create_date\":"
            "\"2026-09-25\",\"read_only\":false}]}");
}

TEST(MssqlFacts, BothPartsShareOneSet) {
  fake_server server;
  nscapi::facts::response out;
  mssql_facts::publish(both(), mssql_facts::gather(both(), server.runner()), 0, out);
  EXPECT_EQ(server.statements.size(), 2u);
  const std::string json = mssql_json(out);
  EXPECT_EQ(json.substr(0, 27), "{\"server_name\":\"DB01\\\\PROD\"");
  EXPECT_NE(json.find("\"databases\":[{\"id\":\"master\""), std::string::npos) << json;
}

TEST(MssqlFacts, AnUnknownValueIsOmittedNotWrittenEmpty) {
  // A default instance on an old server: no instance name, no update level,
  // and the two flags SQL Server 2008 does not have come back NULL. None of
  // them is written as "" or false.
  const mssql_facts::server s = mssql_facts::parse_server(
      make_result(SERVER_COLUMNS, {{"DB02", "DB02", "<null>", "10.50.6000.34", "SP3", "<null>", "Express Edition", "4", "<null>", "0", "<null>", "1"}}));
  mssql_facts::selection what;
  what.server = true;
  mssql_facts::snapshot snap;
  snap.server_info = s;
  nscapi::facts::response out;
  mssql_facts::publish(what, snap, 0, out);
  EXPECT_EQ(mssql_json(out),
            "{\"server_name\":\"DB02\",\"machine_name\":\"DB02\",\"version\":\"10.50.6000.34\",\"product_level\":\"SP3\",\"edition\":\"Express Edition\","
            "\"engine_edition\":\"express\",\"authentication\":\"windows\",\"clustered\":false}");
}

TEST(MssqlFacts, ADateTheServerDidNotRenderIsOmitted) {
  const std::vector<mssql_facts::database> databases = mssql_facts::parse_databases(
      make_result(DATABASE_COLUMNS, {{"odd", "SIMPLE", "<null>", "<null>", "<null>", "1"}, {"", "SIMPLE", "x", "1", "2020-01-01", "0"}}));
  ASSERT_EQ(databases.size(), 1u) << "a row without a name is not a record";
  EXPECT_TRUE(databases[0].create_date.empty());
  EXPECT_TRUE(databases[0].read_only);
  mssql_facts::selection what;
  what.databases = true;
  mssql_facts::snapshot snap;
  snap.databases = databases;
  nscapi::facts::response out;
  mssql_facts::publish(what, snap, 0, out);
  EXPECT_EQ(mssql_json(out), "{\"databases\":[{\"id\":\"odd\",\"recovery_model\":\"SIMPLE\",\"read_only\":true}]}");
}

TEST(MssqlFacts, NoDatabasesIsAnEmptyListNotAMissingSet) {
  mssql_facts::selection what;
  what.databases = true;
  mssql_facts::snapshot snap;
  snap.databases = mssql_facts::parse_databases(make_result(DATABASE_COLUMNS, {}));
  nscapi::facts::response out;
  mssql_facts::publish(what, snap, 0, out);
  EXPECT_EQ(mssql_json(out), "{\"databases\":[]}");
}

TEST(MssqlFacts, NothingSelectedPublishesNothing) {
  nscapi::facts::response out;
  mssql_facts::publish(mssql_facts::selection(), mssql_facts::snapshot(), 0, out);
  EXPECT_EQ(mssql_json(out), "(no such set)");
}

TEST(MssqlFacts, StampsWhenTheValuesWereRead) {
  mssql_facts::selection what;
  what.server = true;
  nscapi::facts::response out;
  mssql_facts::publish(what, mssql_facts::snapshot(), 1790000000, out);
  const PB::Facts::FactsMessage message = out.to_message();
  const PB::Facts::FactSet *set = find_set(message, "mssql");
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(set->gathered(), nscapi::facts::format_time(1790000000));
}

TEST(MssqlFacts, AFailedQueryThrowsRatherThanPublishingAPartialSnapshot) {
  // The properties answer and sys.databases is refused (a login without VIEW
  // ANY DATABASE sees only its own, but a broken one sees an error): nothing
  // is returned, so the module reports an error and the core keeps the last
  // good set.
  const mssql_facts::query_runner half = [](const std::string &sql) -> mssql_odbc::result {
    if (sql.find("SERVERPROPERTY") != std::string::npos) {
      return make_result(SERVER_COLUMNS, {{"DB03", "DB03", "<null>", "15.0.2000.5", "RTM", "<null>", "Developer Edition (64-bit)", "3", "x", "0", "0", "0"}});
    }
    throw mssql_odbc::odbc_exception("[42000/229] The SELECT permission was denied");
  };
  EXPECT_THROW(mssql_facts::gather(both(), half), mssql_odbc::odbc_exception);
  // And a properties query with no row is not a server with no properties.
  EXPECT_THROW(mssql_facts::parse_server(make_result(SERVER_COLUMNS, {})), mssql_odbc::odbc_exception);
}
