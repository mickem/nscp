// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql.hpp"

#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_filter_helpers.hpp"
#include "mssql_options.hpp"

namespace check_mssql_command {

namespace {

const char *HEALTH_SQL =
    "SELECT CAST(SERVERPROPERTY('ServerName') AS nvarchar(256)) AS server_name,"
    " CAST(SERVERPROPERTY('ProductVersion') AS nvarchar(128)) AS version,"
    " CAST(SERVERPROPERTY('ProductLevel') AS nvarchar(128)) AS product_level,"
    " CAST(SERVERPROPERTY('Edition') AS nvarchar(128)) AS edition,"
    " DATEDIFF(second, sqlserver_start_time, GETDATE()) AS uptime"
    " FROM sys.dm_os_sys_info";

typedef server_info filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("server_name", &filter_obj::get_server_name, "Instance name (SERVERPROPERTY('ServerName'))")
      .add_string_var("version", &filter_obj::get_version, "Product version, e.g. 16.0.1000.6")
      .add_string_var("product_level", &filter_obj::get_product_level, "Patch level: RTM, SPn or CUn")
      .add_string_var("edition", &filter_obj::get_edition, "Edition, e.g. Express Edition (64-bit)");

  static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
  registry_.add_int_var("uptime", type_custom_uptime, &filter_obj::get_uptime, "Seconds since the server started (supports units, e.g. uptime < 1h)")
      .add_int_perf("s", "", "_uptime");
  registry_.add_converter(type_custom_uptime, &mssql_filter::parse_time<std::shared_ptr<filter_obj>>);
}

}  // namespace

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;

  filter_type filter;
  // No default thresholds: being able to connect is the health signal, so a
  // reachable server is OK unless the user thresholds version/uptime themselves.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${server_name}: SQL Server ${version} ${product_level} ${edition}, uptime ${uptime}s", "${server_name}",
                           "%(status): No server information returned", "");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(HEALTH_SQL);
    for (std::size_t i = 0; i < res.rows.size(); i++) {
      auto record = std::make_shared<filter_obj>();
      record->server_name = res.get_string(i, "server_name");
      record->version = res.get_string(i, "version");
      record->product_level = res.get_string(i, "product_level");
      record->edition = res.get_string(i, "edition");
      record->uptime = res.get_int(i, "uptime");
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_command
