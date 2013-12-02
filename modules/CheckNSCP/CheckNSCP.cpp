/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "CheckNSCP.h"

#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>

#include <file_helpers.hpp>
#include <unicode_char.hpp>
#include <format.hpp>
#include <file_helpers.hpp>

#include <config.h>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckNSCP::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	start_ =  boost::posix_time::microsec_clock::local_time();

	std::string path;
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, "crash");

	settings.alias().add_path_to_settings()
		("CRASH SECTION", "Configure crash handling properties.")
		;

	settings.alias().add_key_to_settings()
		("archive folder", sh::path_key(&crashFolder, CRASH_ARCHIVE_FOLDER),
		CRASH_ARCHIVE_FOLDER_KEY, "The archive folder for crash dumps.")
		;

	settings.register_all();
	settings.notify();
	NSC_DEBUG_MSG_STD("Crash folder is: " + crashFolder.string());
	return true;
}

bool CheckNSCP::unloadModule() {
	return true;
}
std::string render(int msgType, const std::string file, int line, std::string message) {
	return message;
}
void CheckNSCP::handleLogMessage(const Plugin::LogEntry::Entry &message) {
	if (message.level() != Plugin::LogEntry_Entry_Level_LOG_CRITICAL && message.level() != Plugin::LogEntry_Entry_Level_LOG_ERROR)
		return;
	{
		boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!lock.owns_lock())
			return;
		errors_.push_back(message.message());
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
		if(boost::filesystem::is_regular_file(p) && file_helpers::meta::get_extension(p) == "txt")
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
		return 1;
	}
	if (errors_.empty())
		return 0;
	last_error = errors_.front();
	return errors_.size();
}

void CheckNSCP::check_nscp(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	Plugin::QueryResponseMessage::Response &r = *response;
	po::options_description desc = nscapi::program_options::create_desc(request);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
		return;
	response->set_result(Plugin::Common_ResultCode_OK);
	std::string last, message;
	int crash_count = get_crashes(crashFolder, last);
	format::append_list(message, strEx::s::xtos(crash_count) + " crash(es)", std::string(", "));
	if (crash_count > 0){
		response->set_result(Plugin::Common_ResultCode_CRITCAL);
		format::append_list(message, std::string("last crash: " + last), std::string(", "));
	}

	int err_count = get_errors(last);
	format::append_list(message, strEx::s::xtos(err_count) + " error(s)", std::string(", "));
	if (err_count > 0) {
		response->set_result(Plugin::Common_ResultCode_CRITCAL);
		format::append_list(message, std::string("last error: " + last), std::string(", "));
	}
	boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time();;
	boost::posix_time::time_duration td = end - start_;

	std::stringstream uptime;
	uptime << "uptime " << td;
	format::append_list(message, uptime.str(), std::string(", "));
	response->set_message(message);
}
