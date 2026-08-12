// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mysql.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mysql_filter_helpers.hpp"
#include "mysql_options.hpp"

namespace check_mysql_command {

namespace {

// @@version / @@version_comment / @@max_connections work on MySQL 5.7+,
// MariaDB and Percona alike.
const char *HEALTH_SQL = "SELECT @@version AS version, @@version_comment AS version_comment, @@max_connections AS max_connections";
// Rows of (Variable_name, Value); WHERE ... IN works on every supported flavor.
const char *STATUS_SQL = "SHOW GLOBAL STATUS WHERE Variable_name IN ('Uptime', 'Threads_connected')";

struct filter_obj {
  std::string version;
  std::string version_comment;
  std::string flavor;
  long long uptime = 0;
  long long threads_connected = 0;
  long long max_connections = 0;

  std::string get_version() const { return version; }
  std::string get_version_comment() const { return version_comment; }
  std::string get_flavor() const { return flavor; }
  long long get_uptime() const { return uptime; }
  long long get_threads_connected() const { return threads_connected; }
  long long get_max_connections() const { return max_connections; }
  long long get_connections_pct() const { return max_connections > 0 ? threads_connected * 100 / max_connections : 0; }
  std::string show() const { return flavor + " " + version; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler() {
    registry_.add_string_var("version", &filter_obj::get_version, "Server version, e.g. 10.11.14-MariaDB-ubu2404 or 8.4.3")
        .add_string_var("version_comment", &filter_obj::get_version_comment, "Server version comment (distribution/build description)")
        .add_string_var("flavor", &filter_obj::get_flavor, "Server flavor derived from the version: mysql, mariadb or percona");

    static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
    registry_.add_int_var("uptime", type_custom_uptime, &filter_obj::get_uptime, "Seconds since the server started (supports units, e.g. uptime < 1h)")
        .add_int_perf("s", "", "_uptime");
    registry_.add_converter(type_custom_uptime, &mysql_filter::parse_time<std::shared_ptr<filter_obj>>);

    registry_.add_int_var("threads_connected", &filter_obj::get_threads_connected, "Currently open connections (Threads_connected)")
        .add_int_perf("", "", "_connections")
        .add_int_var("max_connections", &filter_obj::get_max_connections, "Configured connection limit (max_connections)")
        .add_int_var("connections_pct", &filter_obj::get_connections_pct, "Open connections as a percentage of max_connections")
        .add_int_perf("%", "", "_connections_pct");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

}  // namespace

void check_with(const mysql_client::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                PB::Commands::QueryResponseMessage::Response *response, const mysql_client::session_factory &factory) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mysql_client::connection_info info = defaults;

  filter_type filter;
  // No default thresholds: being able to connect is the health signal, so a
  // reachable server is OK unless the user thresholds uptime/connections.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "${flavor} ${version}, uptime ${uptime}s, connections ${threads_connected}/${max_connections} (${connections_pct}%)",
                           "${flavor}", "%(status): No server information returned", "");
  mysql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mysql_options::with_runner(factory, info, response, [&](const mysql_client::query_runner &run) {
    const mysql_client::result health = run(HEALTH_SQL);
    if (health.rows.empty()) throw mysql_client::mysql_exception("Server returned no version information");

    auto record = std::make_shared<filter_obj>();
    record->version = health.get_string(0, "version");
    record->version_comment = health.get_string(0, "version_comment");
    record->max_connections = health.get_int(0, "max_connections");
    record->flavor = mysql_client::derive_flavor(record->version, record->version_comment);

    const mysql_client::result status = run(STATUS_SQL);
    for (std::size_t i = 0; i < status.rows.size(); i++) {
      const std::string name = status.get_string(i, 0);
      if (name == "Uptime") record->uptime = status.get_int(i, 1);
      if (name == "Threads_connected") record->threads_connected = status.get_int(i, 1);
    }

    filter.match(record);
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mysql_command
