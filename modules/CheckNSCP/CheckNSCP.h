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