// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_single_file.hpp"

#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

#include "file_finder.hpp"
#include "filter.hpp"

namespace po = boost::program_options;

namespace check_single_file_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<file_filter::filter> filter_helper(request, response, data);
  std::string file_path;
  bool ignore_missing = false;

  file_filter::filter filter;
  // No "empty" state: a single-file check either has the file (and runs the
  // filter) or it does not (and we return UNKNOWN with a useful message).
  // Default to UNKNOWN if no thresholds are set, so check_single_file <file>
  // by itself confirms the file exists rather than being a silent OK.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  // The top-syntax embeds `%(list)`, which expands to the detail-syntax
  // rendered for each matched item (per-item context). This makes the
  // file's filename, size and age render correctly regardless of whether
  // the status ends up OK, WARNING or CRITICAL — without `%(list)` the
  // top/ok renderers run in aggregate context where per-item columns are
  // unbound and would expand to empty strings (e.g. "WARNING:  (size=0,
  // age=0)"). cli_helper::parse_options_post blanks `renderer_ok` when
  // the top contains `(list)`, so OK status falls through to the top
  // template too — which is exactly what we want here.
  filter_helper.add_syntax("%(status): %(list)", "%(filename) (size=%(size), age=%(age))", "%(filename)",
                           // The "empty" syntaxes below are unreachable for check_single_file (we
                           // always either fail with UNKNOWN or feed exactly one object to the
                           // filter) but cli_helper requires non-empty defaults.
                           "No file inspected", "%(status): %(filename) is ok");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("file", po::value<std::string>(&file_path), "The file to check.")
    ("path", po::value<std::string>(&file_path), "Alias for file.")
    ("ignore-missing", po::value<bool>(&ignore_missing)->implicit_value(true)->default_value(false),
        "Return OK instead of failing when the file does not exist. Intended for files that are legitimately absent some of the time, such as a "
        "lock file or a report that is only written after a run.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (file_path.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No file specified (use file=<path>)");
  }

  if (!filter_helper.build_filter(filter)) return;

  parsers::where::constants::reset();
  const long long now = parsers::where::constants::get_now();
  const std::shared_ptr<file_filter::filter_obj> info = file_finder::stat_single_file(file_path, now);
  if (!info) {
    if (ignore_missing) {
      // Deliberately not routed through the filter: there is no object to
      // match, and saying so plainly beats an empty-syntax line that reads
      // like the file was inspected and found fine.
      nscapi::protobuf::functions::append_simple_query_response_payload(response, "check_single_file", NSCAPI::query_return_codes::returnOK,
                                                                       "File not found (ignored): " + file_path, "");
      return;
    }
    return nscapi::protobuf::functions::set_response_bad(*response, "File not found: " + file_path);
  }

  filter.match(info);
  filter_helper.post_process(filter);
}

}  // namespace check_single_file_command
