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

#include <nscapi/nscapi_core_wrapper.hpp>

#include <Response.h>

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#if BOOST_VERSION >= 105300
#include <boost/atomic/atomic.hpp>
#endif

#include <string>

struct op5_config {
	std::string hostname;
	std::string url;
	std::string username;
	std::string password;
	std::string hostgroups;
	std::string contactgroups;
	typedef std::map<std::string, std::string> check_map;
	check_map checks;
	bool deregister;
	unsigned long long interval;

	op5_config()
		: deregister(false) 
	{}

};

class op5_client {
private:
	const nscapi::core_wrapper *core_;
	int plugin_id_;
	op5_config config_;

#if BOOST_VERSION >= 105300
	boost::atomic<bool> stop_thread_;
#else
	bool stop_thread_;
#endif
	boost::timed_mutex mutex_;
	boost::shared_ptr<boost::thread> thread_;

public:
	op5_client(const nscapi::core_wrapper *core, int plugin_id, op5_config config);
	virtual ~op5_client();
	// Module calls
	void add_check(std::string key, std::string args);
	void stop();
	bool send_a_check(const std::string &alias, int result, std::string message, std::string &status);
private:

	bool has_host(std::string host);
	bool add_host(std::string host, std::string hostgroups, std::string contactgroups);
	bool remove_host(std::string host);
	bool send_host_check(std::string host, int status_code, std::string msg, std::string &status, bool create_if_missing = true);
	bool send_service_check(std::string host, std::string service, int status_code, std::string msg, std::string &status, bool create_if_missing = true);
	std::pair<bool, bool> has_service(std::string service, std::string host, std::string &hosts_string);
	bool add_host_to_service(std::string service, std::string host, std::string &hosts_string);
	bool add_service(std::string host, std::string service);
	bool save_config();

	void register_host(std::string host, std::string hostgroups, std::string contactgroups);
	void deregister_host(std::string host);

	boost::shared_ptr<Mongoose::Response> do_call(const char *verb, const std::string url, const std::string payload);
	void thread_proc();
	const nscapi::core_wrapper* get_core() const {
		return core_;
	}
	unsigned int get_id() const {
		return plugin_id_;
	}


};
