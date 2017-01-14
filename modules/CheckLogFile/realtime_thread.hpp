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

#include "filter_config_object.hpp"

struct real_time_thread {
	boost::shared_ptr<boost::thread> thread_;
	filters::filter_config_handler filters_;
	std::wstring logs_;

#ifdef WIN32
	HANDLE stop_event_;
#else
	int stop_event_[2];
#endif

	nscapi::core_wrapper *core;
	int plugin_id;
	bool enabled_;
	bool debug_;
	bool cache_;

	real_time_thread(nscapi::core_wrapper *core, int plugin_id) : core(core), plugin_id(plugin_id), enabled_(false), debug_(false), cache_(false) {}

	void add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
	void set_enabled(bool flag) { enabled_ = flag; }

	void set_language(std::string lang);
	void set_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string flt) {
		if (!flt.empty())
			add_realtime_filter(proxy, "default", flt);
	}
	bool has_filters() {
		return !filters_.has_objects();
	}
	bool start();
	bool stop();

	void thread_proc();
	void process_object(filters::filter_config_object &object);
	void process_timeout(const filters::filter_config_object &object);
};