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

#include "CheckDisk.h"
#include <time.h>
#include <error/error.hpp>
#include <file_helpers.hpp>
#include <utils.h>

#include <parsers/expression/expression.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/helpers.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include "file_finder.hpp"
#include "filter.hpp"
#include <char_buffer.hpp>
#include <compat.hpp>

#include "check_drive.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

CheckDisk::CheckDisk() : show_errors_(false) {}

void CheckDisk::checkDriveSize(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	boost::program_options::options_description desc;

	std::vector<std::string> times;
	std::vector<std::string> types;
	std::string perf_unit;
	nscapi::program_options::add_help(desc);
	desc.add_options()
		("CheckAll", po::value<std::string>()->implicit_value("true"), "Checks all drives.")
		("CheckAllOthers", po::value<std::string>()->implicit_value("true"), "Checks all drives turns the drive option into an exclude option.")
		("Drive", po::value<std::vector<std::string>>(&times), "The drives to check")
		("FilterType", po::value<std::vector<std::string>>(&types), "The type of drives to check fixed, remote, cdrom, ramdisk, removable")
		("perf-unit", po::value<std::string>(&perf_unit), "Force performance data to use a given unit prevents scaling which can cause problems over time in some graphing solutions.")
		;
	compat::addShowAll(desc);
	compat::addAllNumeric(desc);
	compat::addAllNumeric(desc, "Free");
	compat::addAllNumeric(desc, "Used");

	boost::program_options::variables_map vm;
	std::vector<std::string> extra;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response, true, extra))
		return;
	std::string warn, crit;

	request.clear_arguments();
	compat::matchFirstNumeric(vm, "used", "free", warn, crit);
	compat::matchFirstNumeric(vm, "used", "used", warn, crit, "Used");
	compat::matchFirstNumeric(vm, "free", "free", warn, crit, "Free");
	compat::inline_addarg(request, warn);
	compat::inline_addarg(request, crit);
	if (vm.count("CheckAll"))
		request.add_arguments("drive=*");
	bool exclude = false;
	if (vm.count("CheckAllOthers")) {
		request.add_arguments("drive=*");
		exclude = true;
	}
	if (!perf_unit.empty())
		request.add_arguments("perf-config=free(unit:" + perf_unit + ")used(unit:" + perf_unit + ")");
	request.add_arguments("detail-syntax=%(drive): Total: %(size) - Used: %(used) (%(used_pct)%) - Free: %(free) (%(free_pct)%)");
	compat::matchShowAll(vm, request);
	std::string keyword = exclude ? "exclude=" : "drive=";
	BOOST_FOREACH(const std::string &t, times) {
		request.add_arguments(keyword + t);
	}
	BOOST_FOREACH(const std::string &t, extra) {
		request.add_arguments(keyword + t);
	}

	if (!types.empty()) {
		std::string type_list = "";
		BOOST_FOREACH(const std::string &s, types) {
			if (!type_list.empty())
				type_list += ", ";
			type_list += "'" + s + "'";
		}
		request.add_arguments("filter=type in (" + type_list + ")");
	}
	compat::log_args(request);
	check_drive::check(request, response);
}

void CheckDisk::check_drivesize(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	check_drive::check(request, response);
}

void CheckDisk::checkFiles(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	boost::program_options::options_description desc;

	std::vector<std::string> times;
	std::vector<std::string> types;
	std::string syntax = "${filename}";
	std::string master_syntax = "${list}";
	std::string path;
	std::string pattern;
	std::string filter;
	std::string warn2;
	std::string crit2;
	bool debug = false;
	int maxDepth = 0;
	nscapi::program_options::add_help(desc);
	desc.add_options()
		("syntax", po::value<std::string>(&syntax), "Syntax for individual items (detail-syntax).")
		("master-syntax", po::value<std::string>(&master_syntax), "Syntax for top syntax (top-syntax).")
		("path", po::value<std::string>(&path), "The file or path to check")
		("pattern", po::value<std::string>(&pattern), "Deprecated and ignored")
		("alias", po::value<std::string>(), "Deprecated and ignored")
		("debug", po::bool_switch(&debug), "Debug")
		("max-dir-depth", po::value<int>(&maxDepth), "The maximum level to recurse")
		("filter", po::value<std::string>(&filter), "The filter to use when including files in the check")
		("warn", po::value<std::string>(&warn2), "Deprecated and ignored")
		("crit", po::value<std::string>(&crit2), "Deprecated and ignored")
		;
	compat::addAllNumeric(desc);

	boost::program_options::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	std::string warn, crit;

	request.clear_arguments();
	compat::matchFirstNumeric(vm, "count", "count", warn, crit);
	if (!warn.empty() && !warn2.empty()) {
		NSC_LOG_ERROR("Duplicate warnings not supported.");
	} else if (!warn2.empty()) {
		boost::replace_all(warn2, ":", " ");
		warn = "warn=count " + warn2;
	}
	if (!crit.empty() && !crit2.empty()) {
		NSC_LOG_ERROR("Duplicate warnings not supported.");
	} else if (!crit2.empty()) {
		boost::replace_all(crit2, ":", " ");
		crit = "crit=count " + crit2;
	}
	compat::inline_addarg(request, warn);
	compat::inline_addarg(request, crit);
	compat::inline_addarg(request, "filter=", filter);
	compat::inline_addarg(request, "pattern=", pattern);

	boost::replace_all(syntax, "%filename%", "%(filename)");
	boost::replace_all(syntax, "%size%", "%(size)");
	boost::replace_all(syntax, "%write%", "%(written)");
	compat::inline_addarg(request, "detail-syntax=", syntax);

	boost::replace_all(master_syntax, "%list%", "%(list)");
	boost::replace_all(master_syntax, "%count%", "%(count)");
	boost::replace_all(master_syntax, "%total%", "%(total)");
	compat::inline_addarg(request, "top-syntax=", master_syntax);
	compat::inline_addarg(request, "path=", path);
	if (debug)
		request.add_arguments("debug");
	if (maxDepth > 0)
		request.add_arguments("max-depth=" + str::xtos(maxDepth));
	compat::log_args(request);
	check_files(request, response);
}

void CheckDisk::check_files(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	modern_filter::data_container data;
	modern_filter::cli_helper<file_filter::filter> filter_helper(request, response, data);
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;
	bool ignoreError = false;
	file_finder::scanner_context context;
	context.max_depth = -1;
	std::string total;

	file_filter::filter filter;
	filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
	filter_helper.add_syntax("${status}: ${problem_count}/${count} files (${problem_list})", "${name}", "${name}", "No files found", "%(status): All %(count) files are ok");
	filter_helper.get_desc().add_options()
		("path", po::value<std::vector<std::string> >(&file_list), "The path to search for files under.\nNotice that specifying multiple path will create an aggregate set you will not check each path individually."
			"In other words if one path contains an error the entire check will result in error.")
		("file", po::value<std::vector<std::string> >(&file_list), "Alias for path.")
		("paths", po::value<std::string>(&files_string), "A comma separated list of paths to scan")
		("pattern", po::value<std::string>(&context.pattern)->default_value("*.*"), "The pattern of files to search for (works like a filter but is faster and can be combined with a filter).")
		("max-depth", po::value<int>(&context.max_depth), "Maximum depth to recurse")
		("total", po::value(&total)->implicit_value("filter"), "Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter")
		;

	context.now = parsers::where::constants::get_now();

	if (!filter_helper.parse_options())
		return;

	if (!files_string.empty())
		boost::split(file_list, files_string, boost::is_any_of(","));

	if (file_list.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No path specified");

	if (!filter_helper.build_filter(filter))
		return;

	boost::shared_ptr<file_filter::filter_obj> total_obj;
	if (!total.empty())
		total_obj = file_filter::filter_obj::get_total(context.now);

	BOOST_FOREACH(const std::string &path, file_list) {
		file_finder::recursive_scan(filter, context, path, total_obj, total == "all");
	}
	if (total_obj) {
		filter.match(total_obj);
	}
	filter_helper.post_process(filter);
}