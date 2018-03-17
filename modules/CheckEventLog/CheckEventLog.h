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

#include "bookmarks.hpp"
#include "filter.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

#include <boost/shared_ptr.hpp>

struct real_time_thread;
class CheckEventLog : public nscapi::impl::simple_plugin {
private:
	boost::shared_ptr<real_time_thread> thread_;
	bool debug_;
	std::string syntax_;
	int buffer_length_;
	bool lookup_names_;
	bookmarks bookmarks_;

public:
	CheckEventLog() {}
	virtual ~CheckEventLog() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void parse(std::wstring expr);

	void check_eventlog(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void CheckEventLog_(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);

	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void insert_eventlog(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	void list_providers(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	void add_filter(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);


private:
	void save_bookmark(const std::string bookmark, eventlog::api::EVT_HANDLE &hResults);
	void check_modern(const std::string &logfile, const std::string &scan_range, const int truncate_message, eventlog_filter::filter &filter, std::string bookmark);
};
