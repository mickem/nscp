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

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include "eventlog_wrapper.hpp"
#include "eventlog_record.hpp"
#include "filter_config_object.hpp"

struct real_time_thread {
	nscapi::core_wrapper *core;
	int plugin_id;
	bool enabled_;
	unsigned long long start_age_;
	boost::shared_ptr<boost::thread> thread_;
	HANDLE stop_event_;
	eventlog_filter::filter_config_handler filters_;
	std::string logs_;

	bool cache_;
	bool debug_;

	real_time_thread(nscapi::core_wrapper *core, int plugin_id) : core(core), plugin_id(plugin_id), enabled_(false), start_age_(0), debug_(false), cache_(false) {
		set_start_age("30m");
	}

	void add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
	void set_enabled(bool flag) { enabled_ = flag; }
	void set_start_age(std::string age) {
		start_age_ = str::format::stox_as_time_sec<unsigned long long>(age, "s");
	}

	void set_language(std::string lang);
	void set_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string flt) {
		if (!flt.empty())
			add_realtime_filter(proxy, "default", flt);
	}
	bool has_filters() {
		return !filters_.has_objects();
	}

	void set_path(const std::string &p);
	bool start();
	bool stop();

	void thread_proc();
	//	void process_events(eventlog_filter::filter_engine engine, eventlog_wrapper &eventlog);
	void process_no_events(const eventlog_filter::filter_config_object &object);
	void process_record(eventlog_filter::filter_config_object &object, const EventLogRecord &record);
	void debug_miss(const EventLogRecord &record);
	//	void process_event(eventlog_filter::filter_engine engine, const EVENTLOGRECORD* record);
};