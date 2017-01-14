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

#pragma once

#include <string>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/plugin.hpp>

#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

class CheckNSCP : public nscapi::impl::simple_plugin {
private:
	boost::timed_mutex mutex_;
	boost::filesystem::path crashFolder;
	//typedef std::list<std::string> error_list;
	//error_list errors_;
	std::string last_error_;
	unsigned int error_count_;
	boost::posix_time::ptime start_;
public:

	CheckNSCP() : error_count_(0) {}

	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void check_nscp(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_nscp_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void handleLogMessage(const Plugin::LogEntry::Entry &message);

	std::size_t get_errors(std::string &last_error);
};