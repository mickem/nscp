// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mysql_facts.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>
#include <string>
#include <vector>

// The plugin singleton is defined in check_mysql_test.cpp, which is compiled
// into the same test binary.

namespace {

using nscapi::facts::testing::gathered_of;
using nscapi::facts::testing::json_of;

std::string mysql_json(const nscapi::facts::response &out) { return json_of(out, "mysql"); }

mysql_client::result make_result(const std::vector<std::string> &columns, const std::vector<std::vector<std::string>> &rows) {
  mysql_client::result res;
  res.columns = columns;
  for (const std::vector<std::string> &row : rows) {
    std::vector<mysql_client::cell> cells;
    for (const std::string &text : row) {
      // "<null>" is a NULL cell, which the session leaves with empty text.
      mysql_client::cell c;
      c.null = text == "<null>";
      c.text = c.null ? "" : text;
      cells.push_back(c);
    }
    res.rows.push_back(cells);
  }
  return res;
}

const std::vector<std::string> SERVER_COLUMNS = {"version", "version_comment", "hostname", "port", "server_id", "character_set", "collation", "os", "machine"};

// A MariaDB server as a distro ships it, plus the four schemas every server
// has and one of the operator's - in the storage order a server without the
// ORDER BY might hand back.
struct fake_server {
  std::vector<std::string> statements;

  mysql_client::query_runner runner() {
    return [this](const std::string &sql) -> mysql_client::result {
      statements.push_back(sql);
      if (sql.find("@@version") != std::string::npos) {
        return make_result(SERVER_COLUMNS,
                           {{"10.11.14-MariaDB-ubu2404", "Ubuntu 24.04", "db01", "3306", "1", "utf8mb4", "utf8mb4_general_ci", "debian-linux-gnu", "x86_64"}});
      }
      if (sql.find("SCHEMATA") != std::string::npos) {
        return make_result({"name", "character_set", "collation"}, {{"shop", "utf8mb4", "utf8mb4_unicode_ci"},
                                                                    {"information_schema", "utf8mb3", "utf8mb3_general_ci"},
                                                                    {"mysql", "utf8mb4", "utf8mb4_general_ci"},
                                                                    {"performance_schema", "utf8mb4", "utf8mb4_general_ci"},
                                                                    {"sys", "utf8mb4", "utf8mb4_general_ci"}});
      }
      throw mysql_client::mysql_exception("unexpected query: " + sql);
    };
  }
};

mysql_facts::selection both() {
  mysql_facts::selection what;
  what.server = what.databases = true;
  return what;
}

}  // namespace

TEST(MysqlFacts, ServerRecordCarriesTheServerNotItsLoad) {
  fake_server server;
  mysql_facts::selection what;
  what.server = true;
  const mysql_facts::snapshot snap = mysql_facts::gather(what, server.runner());
  EXPECT_EQ(server.statements.size(), 1u);
  EXPECT_EQ(snap.server_info.flavor, "mariadb");

  nscapi::facts::response out;
  mysql_facts::publish(what, snap, 0, out);
  // No uptime, no connection counts: those change every round and belong to
  // check_mysql. And nothing from [/settings/mysql]: the hostname and port
  // are what the server says about itself.
  EXPECT_EQ(mysql_json(out),
            "{\"flavor\":\"mariadb\",\"version\":\"10.11.14-MariaDB-ubu2404\",\"version_comment\":\"Ubuntu 24.04\",\"hostname\":\"db01\",\"port\":3306,"
            "\"server_id\":1,\"character_set\":\"utf8mb4\",\"collation\":\"utf8mb4_general_ci\",\"os\":\"debian-linux-gnu\",\"architecture\":\"x86_64\"}");
}

TEST(MysqlFacts, ArchitectureIsNormalisedToTheOsVocabulary) {
  const mysql_facts::server s = mysql_facts::parse_server(
      make_result(SERVER_COLUMNS, {{"8.4.3", "MySQL Community Server - GPL", "db02", "3306", "2", "utf8mb4", "utf8mb4_0900_ai_ci", "Linux", "aarch64"}}));
  EXPECT_EQ(s.flavor, "mysql");
  EXPECT_EQ(s.architecture, "arm64");
}

TEST(MysqlFacts, DatabasesAreRecordsBySchemaNameSorted) {
  fake_server server;
  mysql_facts::selection what;
  what.databases = true;
  const mysql_facts::snapshot snap = mysql_facts::gather(what, server.runner());
  EXPECT_EQ(server.statements.size(), 1u) << "only the list was asked for";
  ASSERT_EQ(snap.databases.size(), 5u);
  EXPECT_EQ(snap.databases[0].id, "information_schema");
  EXPECT_EQ(snap.databases[4].id, "sys");

  nscapi::facts::response out;
  mysql_facts::publish(what, snap, 0, out);
  EXPECT_EQ(mysql_json(out),
            "{\"databases\":[{\"id\":\"information_schema\",\"character_set\":\"utf8mb3\",\"collation\":\"utf8mb3_general_ci\"},"
            "{\"id\":\"mysql\",\"character_set\":\"utf8mb4\",\"collation\":\"utf8mb4_general_ci\"},"
            "{\"id\":\"performance_schema\",\"character_set\":\"utf8mb4\",\"collation\":\"utf8mb4_general_ci\"},"
            "{\"id\":\"shop\",\"character_set\":\"utf8mb4\",\"collation\":\"utf8mb4_unicode_ci\"},"
            "{\"id\":\"sys\",\"character_set\":\"utf8mb4\",\"collation\":\"utf8mb4_general_ci\"}]}");
}

TEST(MysqlFacts, BothPartsShareOneSet) {
  fake_server server;
  nscapi::facts::response out;
  mysql_facts::publish(both(), mysql_facts::gather(both(), server.runner()), 0, out);
  EXPECT_EQ(server.statements.size(), 2u);
  const std::string json = mysql_json(out);
  EXPECT_EQ(json.rfind("{\"flavor\":\"mariadb\",", 0), 0u) << json.substr(0, 80);
  EXPECT_NE(json.find("\"databases\":[{\"id\":\"information_schema\""), std::string::npos) << json;
}

TEST(MysqlFacts, AnUnknownValueIsOmittedNotWrittenEmpty) {
  // A NULL hostname (a server built without one), a zero server_id and an
  // empty comment are absent from the record, never "" or 0.
  const mysql_facts::server s = mysql_facts::parse_server(make_result(SERVER_COLUMNS, {{"8.0.36", "", "<null>", "0", "0", "utf8mb4", "<null>", "", ""}}));
  mysql_facts::selection what;
  what.server = true;
  mysql_facts::snapshot snap;
  snap.server_info = s;
  nscapi::facts::response out;
  mysql_facts::publish(what, snap, 0, out);
  EXPECT_EQ(mysql_json(out), "{\"flavor\":\"mysql\",\"version\":\"8.0.36\",\"character_set\":\"utf8mb4\"}");
}

TEST(MysqlFacts, ALongListIsCutAndTheSetSaysSo) {
  // More databases than the set will ship: the first max_records go out,
  // the rest is counted in the set's error, and the set - server record
  // included - is still published rather than rejected whole by the core.
  mysql_facts::snapshot snap;
  snap.server_info.flavor = "mariadb";
  for (std::size_t n = 0; n < mysql_facts::max_records + 1; ++n) {
    mysql_facts::database d;
    d.id = "db" + std::to_string(n);
    snap.databases.push_back(d);
  }
  nscapi::facts::response out;
  mysql_facts::publish(both(), snap, 0, out);
  const std::string json = mysql_json(out);
  EXPECT_EQ(json.rfind("{\"flavor\":\"mariadb\",", 0), 0u) << json.substr(0, 80);
  EXPECT_EQ(static_cast<std::size_t>(std::count(json.begin(), json.end(), '{')), 1 + mysql_facts::max_records) << "one object per record, plus the set";
  EXPECT_EQ(nscapi::facts::testing::error_of(out, "mysql"), "Server has " + std::to_string(mysql_facts::max_records + 1) + " databases; only the first " +
                                                                std::to_string(mysql_facts::max_records) +
                                                                " are reported, to keep the facts document inside its size budget");
}

TEST(MysqlFacts, NoDatabasesIsAnEmptyListNotAMissingSet) {
  mysql_facts::selection what;
  what.databases = true;
  mysql_facts::snapshot snap;
  snap.databases = mysql_facts::parse_databases(make_result({"name", "character_set", "collation"}, {}));
  nscapi::facts::response out;
  mysql_facts::publish(what, snap, 0, out);
  EXPECT_EQ(mysql_json(out), "{\"databases\":[]}");
}

TEST(MysqlFacts, NothingSelectedPublishesNothing) {
  nscapi::facts::response out;
  mysql_facts::publish(mysql_facts::selection(), mysql_facts::snapshot(), 0, out);
  EXPECT_EQ(mysql_json(out), "(no such set)");
}

TEST(MysqlFacts, StampsWhenTheValuesWereRead) {
  mysql_facts::selection what;
  what.server = true;
  nscapi::facts::response out;
  mysql_facts::publish(what, mysql_facts::snapshot(), 1790000000, out);
  EXPECT_EQ(gathered_of(out, "mysql"), nscapi::facts::format_time(1790000000));
}

TEST(MysqlFacts, AFailedQueryThrowsRatherThanPublishingAPartialSnapshot) {
  // The version query answers and the schema listing is refused (a user
  // without SHOW DATABASES): nothing is returned, so the module reports an
  // error and the core keeps the last good set.
  const mysql_client::query_runner half = [](const std::string &sql) -> mysql_client::result {
    if (sql.find("@@version") != std::string::npos) {
      return make_result(SERVER_COLUMNS, {{"8.4.3", "MySQL Community Server - GPL", "db02", "3306", "2", "utf8mb4", "utf8mb4_0900_ai_ci", "Linux", "x86_64"}});
    }
    throw mysql_client::mysql_exception("Access denied");
  };
  EXPECT_THROW(mysql_facts::gather(both(), half), mysql_client::mysql_exception);
  // And a version query with no row is not a server with no version.
  EXPECT_THROW(mysql_facts::parse_server(make_result(SERVER_COLUMNS, {})), mysql_client::mysql_exception);
}
