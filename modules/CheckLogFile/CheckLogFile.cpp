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

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include "CheckLogFile.h"
#include "realtime_thread.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckLogFile::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	thread_.reset(new real_time_thread(get_core(), get_id()));

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, "logfile");

	thread_->filters_.set_path(settings.alias().get_settings_path("real-time/checks"));

	settings.alias().add_path_to_settings()

		("real-time", "Real-time filtering", "A set of options to configure the real time checks")

		("real-time/checks", sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, thread_, get_settings_proxy(), _1, _2)),
			"Real-time filters", "A set of filters to use in real-time mode",
			"REALTIME FILTER DEFENTION", "For more configuration options add a dedicated section"
			)
		;

	settings.alias().add_key_to_settings("real-time")

		("enabled", sh::bool_fun_key(boost::bind(&real_time_thread::set_enabled, thread_, _1), false),
			"Real time", "Spawns a background thread which waits for file changes.")

		;

	settings.register_all();
	settings.notify();


	thread_->filters_.add_samples(get_settings_proxy());
	if (mode == NSCAPI::normalStart) {
		if (!thread_->start())
			NSC_LOG_ERROR_STD("Failed to start collection thread");
	}
	return true;
}
bool CheckLogFile::unloadModule() {
	if (thread_ && !thread_->stop())
		NSC_LOG_ERROR_STD("Failed to stop thread");
	return true;
}

void CheckLogFile::check_logfile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef logfile_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::string regexp, line_split, column_split;
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;

	filter_type filter;
	filter_helper.add_options("", "", "", filter.get_filter_syntax());
	filter_helper.add_syntax("${count}/${total} (${problem_list})", "${column1}", "${column1}", "%(status): Nothing found", "");
	filter_helper.get_desc().add_options()
		//		("regexp", po::value<std::string>(&regexp),					"Lookup a numeric value in the PDH index table")
		("line-split", po::value<std::string>(&line_split)->default_value("\\n"),
			"Character string used to split a file into several lines (default \\n)")
		("column-split", po::value<std::string>(&column_split)->default_value("\\t"),
			"Character string to split a line into several columns (default \\t)")
		("split", po::value<std::string>(&column_split), "Alias for split-column")
		("file", po::value<std::vector<std::string> >(&file_list), "File to read (can be specified multiple times to check multiple files.\n"
			"Notice that specifying multiple files will create an aggregate set it will not check each file individually.\n"
			"In other words if one file contains an error the entire check will result in error or if you check the count it is the global count which is used.")
		("files", po::value<std::string>(&files_string), "A comma separated list of files to scan (same as file except a list)")
		//		("mode", po::value<std::string>(&mode),						"Mode of operation: count (count all critical/warning lines), find (find first critical/warning line)")
		;

	if (!files_string.empty())
		boost::split(file_list, files_string, boost::is_any_of(","));

	if (!filter_helper.parse_options())
		return;

	if (column_split.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No column-split specified");
	if (line_split.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No line-split specified");

	if (file_list.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "Need to specify at least one file: file=foo.txt");

	str::utils::replace(column_split, "\\t", "\t");
	str::utils::replace(column_split, "\\n", "\n");
	str::utils::replace(line_split, "\\t", "\t");
	str::utils::replace(line_split, "\\n", "\n");

	if (!filter_helper.build_filter(filter))
		return;

	BOOST_FOREACH(const std::string &filename, file_list) {
		std::ifstream file(filename.c_str());
		if (file.is_open()) {
			std::string line;
			while (file.good()) {
				std::getline(file, line, '\n');
				std::list<std::string> chunks = str::utils::split_lst(line, column_split);
				boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(filename, line, chunks));
				filter.match(record);
			}
			file.close();
		} else {
			return nscapi::protobuf::functions::set_response_bad(*response, "Failed to open file: " + filename);
		}
	}
	filter_helper.post_process(filter);
}