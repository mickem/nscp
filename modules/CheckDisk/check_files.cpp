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

#include "check_files.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/shared_ptr.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

#include "file_finder.hpp"
#include "filter.hpp"

namespace po = boost::program_options;

namespace check_files_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<file_filter::filter> filter_helper(request, response, data);
  std::vector<std::string> file_list;
  std::string files_string;
  std::string mode;
  file_finder::scanner_context context;
  context.max_depth = -1;
  std::string total;

  file_filter::filter filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_count}/${count} files (${problem_list})", "${name}", "${name}", "No files found",
                           "%(status): All %(count) files are ok");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("path", po::value<std::vector<std::string> >(&file_list), "The path to search for files under.\nNotice that specifying multiple path will create an aggregate set you will not check each path individually."
        "In other words if one path contains an error the entire check will result in error.")
    ("file", po::value<std::vector<std::string> >(&file_list), "Alias for path.")
    ("paths", po::value<std::string>(&files_string), "A comma separated list of paths to scan")
    ("pattern", po::value<std::string>(&context.pattern)->default_value("*.*"), "The pattern of files to search for (works like a filter but is faster and can be combined with a filter).")
    ("max-depth", po::value<int>(&context.max_depth), "Maximum depth to recurse")
    ("total", po::value(&total)->implicit_value("filter"), "Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter")
  ;
  // clang-format on

  context.now = parsers::where::constants::get_now();

  if (!filter_helper.parse_options()) return;

  if (!files_string.empty()) boost::split(file_list, files_string, boost::is_any_of(","));

  if (file_list.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No path specified");

  if (!filter_helper.build_filter(filter)) return;

  boost::shared_ptr<file_filter::filter_obj> total_obj;
  if (!total.empty()) total_obj = file_filter::filter_obj::get_total(context.now);

  for (const std::string &path : file_list) {
    file_finder::recursive_scan(filter, context, path, total_obj, total == "all");
  }
  if (!context.missing_paths.empty()) {
    // One or more user-supplied top-level paths could not be opened. Surface
    // this as UNKNOWN with the offending path(s) so operators see a clear
    // error instead of a misleading OK / "No files found" (issue #613).
    std::string joined;
    for (const std::string &p : context.missing_paths) {
      if (!joined.empty()) joined += ", ";
      joined += p;
    }
    return nscapi::protobuf::functions::set_response_bad(*response, "Path was not found: " + joined);
  }
  if (total_obj) {
    filter.match(total_obj);
  }
  filter_helper.post_process(filter);
}

}  // namespace check_files_command
