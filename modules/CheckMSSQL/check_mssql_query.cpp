// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mssql_query.hpp"

#include <boost/program_options.hpp>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "mssql_options.hpp"

namespace po = boost::program_options;

namespace check_mssql_query_command {

namespace {

// One result-set row; columns are resolved by name against the shared result.
struct filter_obj {
  const mssql_odbc::result &res;
  std::size_t row;
  filter_obj(const mssql_odbc::result &res, std::size_t row) : res(res), row(row) {}

  std::string get_string(const std::string &col) const { return res.get_string(row, col); }
  long long get_int(const std::string &col) const { return res.get_int(row, col); }
  std::string get_row() const {
    std::string line;
    for (const std::string &col : res.columns) {
      if (!line.empty()) line += ", ";
      line += col + "=" + res.get_string(row, col);
    }
    return line;
  }
  std::string show() const { return get_row(); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler() = default;
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

}  // namespace

void check(const mssql_odbc::connection_info &defaults, const PB::Commands::QueryRequestMessage::Request &request,
           PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  mssql_odbc::connection_info info = defaults;
  std::string query;

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${list}", "%(line)", "", "", "");
  filter_helper.get_desc().add_options()("query", po::value<std::string>(&query), "The T-SQL query to execute.");
  mssql_options::add_connection_options(filter_helper.get_desc(), info);

  if (!filter_helper.parse_options()) return;

  if (query.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No query specified (use query=<T-SQL>)");

  mssql_options::with_session(info, response, [&](mssql_odbc::session &session) {
    const mssql_odbc::result res = session.execute(query);

    // Register the returned columns as filter keywords, CheckWMI-style: each
    // column is usable in filter/warn/crit expressions and as perfdata.
    filter.context->registry_.add_string_var("line", &filter_obj::get_row, "All columns of the row as column=value pairs");
    for (const std::string &col : res.columns) {
      filter.context->registry_
          .add_int_var(
              col, [col](const auto &obj) { return obj->get_int(col); }, [col](const auto &obj) { return obj->get_string(col); }, "Column: " + col)
          .add_int_perf("", col, "");
    }

    if (!filter_helper.build_filter(filter)) return;

    for (std::size_t i = 0; i < res.rows.size(); i++) {
      auto record = std::make_shared<filter_obj>(res, i);
      filter.match(record);
    }
    filter_helper.post_process(filter);
  });
}

}  // namespace check_mssql_query_command
