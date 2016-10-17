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
		start_age_ = strEx::stoi64_as_time(age);
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