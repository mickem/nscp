/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CheckNSCP.h"

#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>

#include <file_helpers.hpp>
#include <format.hpp>
#include <file_helpers.hpp>
#include <common.hpp>
#include <config.h>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <error/nscp_exception.hpp>

#include <parsers/filter/modern_filter.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/helpers.hpp>


#include <nscapi/nscapi_settings_helper.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckNSCP::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	start_ = boost::posix_time::microsec_clock::local_time();

	std::string path;
	sh::settings_registry settings(get_settings_proxy());
	crashFolder = get_core()->expand_path(CRASH_ARCHIVE_FOLDER);
	NSC_DEBUG_MSG_STD("Crash folder is: " + crashFolder.string());
	return true;
}

bool CheckNSCP::unloadModule() {
	return true;
}
std::string render(int, const std::string, int, std::string message) {
	return message;
}
void CheckNSCP::handleLogMessage(const Plugin::LogEntry::Entry &message) {
	if (message.level() != Plugin::LogEntry_Entry_Level_LOG_CRITICAL && message.level() != Plugin::LogEntry_Entry_Level_LOG_ERROR)
		return;
	{
		boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!lock.owns_lock())
			return;
		error_count_++;
		last_error_ = message.message();
	}
}

int get_crashes(boost::filesystem::path root, std::string &last_crash) {
	if (!boost::filesystem::is_directory(root)) {
		return 0;
	}
	int count = 0;

	time_t last_write = std::time(0);
	boost::filesystem::directory_iterator begin(root), end;
	BOOST_FOREACH(const boost::filesystem::path& p, std::make_pair(begin, end)) {
		if (boost::filesystem::is_regular_file(p) && file_helpers::meta::get_extension(p) == "txt")
			count++;
		time_t lw = boost::filesystem::last_write_time(p);
		if (lw > last_write) {
			last_write = lw;
			last_crash = file_helpers::meta::get_filename(p);
		}
	}
	return count;
}

std::size_t CheckNSCP::get_errors(std::string &last_error) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		last_error = "Failed to get lock";
		return error_count_ + 1;
	}
	last_error = last_error_;
	return error_count_;
}


struct nscp_version {
	int release;
	int major_version;
	int minor_version;
	int build;
	std::string date;

	nscp_version() : release(0), major_version(0), minor_version(0), build(0) {}
	nscp_version(const nscp_version &other) : release(other.release), major_version(other.major_version), minor_version(other.minor_version), build(other.build), date(other.date) {}
	nscp_version& operator= (const nscp_version &other) {
		release = other.release;
		major_version = other.major_version;
		minor_version = other.minor_version;
		build = other.build;
		date = other.date;
		return *this;
	}
	nscp_version(std::string v) {
		boost::tuple<std::string,std::string> v2 = strEx::s::split2(v, " ");
		date = v2.get<1>();
		std::list<std::string> vl = strEx::s::splitEx(v2.get<0>(), ".");
		if (vl.size() != 4)
			throw error::nscp_exception("Failed to parse version: " + v);
		release = strEx::s::stox<int>(vl.front()); vl.pop_front();
		major_version = strEx::s::stox<int>(vl.front()); vl.pop_front();
		minor_version = strEx::s::stox<int>(vl.front()); vl.pop_front();
		build = strEx::s::stox<int>(vl.front());
	}
	std::string to_string() const {
		return strEx::s::xtos(release) + "."
			+ strEx::s::xtos(major_version) + "."
			+ strEx::s::xtos(minor_version) + "."
			+ strEx::s::xtos(build);
	}
};


namespace check_nscp_version {
	struct filter_obj {
		nscp_version version;

		filter_obj() {}
		filter_obj(nscp_version version) : version(version) {}

		long long get_major(parsers::where::evaluation_context context) const {
			return version.major_version;
		}
		long long get_minor(parsers::where::evaluation_context context) const {
			return version.minor_version;
		}
		long long get_build(parsers::where::evaluation_context context) const {
			return version.build;
		}
		long long get_release(parsers::where::evaluation_context context) const {
			return version.release;
		}
		std::string get_version_s(parsers::where::evaluation_context context) const {
			return version.to_string();
		}
		std::string get_date_s(parsers::where::evaluation_context context) const {
			return version.date;
		}
	};
	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

	struct filter_obj_handler : public native_context {

		filter_obj_handler() {
			registry_.add_string()
				("version", &filter_obj::get_version_s, "The NSClient++ Version as a string")
				("date", &filter_obj::get_date_s, "The NSClient++ Build date")
				;
			registry_.add_int()
				("release", &filter_obj::get_release, "The release (the 0 in 0.1.2.3)")
				("major", &filter_obj::get_major, "The major (the 1 in 0.1.2.3)")
				("minor", &filter_obj::get_minor, "The minor (the 2 in 0.1.2.3)")
				("build", &filter_obj::get_build, "The build (the 3 in 0.1.2.3)")
				;
		}
	};

	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

	void check(const nscp_version &version, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
		typedef filter filter_type;
		modern_filter::data_container data;
		modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

		filter_type filter;
		filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
		filter_helper.add_syntax("${status}: ${list}", filter.get_filter_syntax(), "${release}.${major}.${minor}.${build} (${date})", "version", "", "");

		if (!filter_helper.parse_options())
			return;

		if (!filter_helper.build_filter(filter))
			return;

		boost::shared_ptr<filter_obj> record(new filter_obj(version));
		filter.match(record);

		filter_helper.post_process(filter);
	}

}




void CheckNSCP::check_nscp_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	nscp_version version;
	try {
		version = nscp_version(get_core()->getApplicationVersionString());
	} catch (const error::nscp_exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to parse version: " + e.reason());
		return;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::set_response_bad(*response, "Failed to parse version: " + utf8::utf8_from_native(e.what()));
		return;
	}
	check_nscp_version::check(version, request, response);
}

void CheckNSCP::check_nscp(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	po::options_description desc = nscapi::program_options::create_desc(request);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	response->set_result(Plugin::Common_ResultCode_OK);
	std::string last, message;
	int crash_count = get_crashes(crashFolder, last);
	format::append_list(message, strEx::s::xtos(crash_count) + " crash(es)", std::string(", "));
	if (crash_count > 0) {
		response->set_result(Plugin::Common_ResultCode_CRITICAL);
		format::append_list(message, std::string("last crash: " + last), std::string(", "));
	}

	int err_count = get_errors(last);
	format::append_list(message, strEx::s::xtos(err_count) + " error(s)", std::string(", "));
	if (err_count > 0) {
		response->set_result(Plugin::Common_ResultCode_CRITICAL);
		format::append_list(message, std::string("last error: " + last), std::string(", "));
	}
	boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time();;
	boost::posix_time::time_duration td = end - start_;

	std::stringstream uptime;
	uptime << "uptime " << td;
	format::append_list(message, uptime.str(), std::string(", "));
	response->add_lines()->set_message(message);
}