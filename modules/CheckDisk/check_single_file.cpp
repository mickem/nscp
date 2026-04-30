/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "check_single_file.hpp"

#include <boost/shared_ptr.hpp>
#include <nscapi/nscapi_program_options.hpp>
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
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (file_path.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No file specified (use file=<path>)");
  }

  if (!filter_helper.build_filter(filter)) return;

  const long long now = parsers::where::constants::get_now();
  const boost::shared_ptr<file_filter::filter_obj> info = file_finder::stat_single_file(file_path, now);
  if (!info) {
    return nscapi::protobuf::functions::set_response_bad(*response, "File not found: " + file_path);
  }

  filter.match(info);
  filter_helper.post_process(filter);
}

}  // namespace check_single_file_command
