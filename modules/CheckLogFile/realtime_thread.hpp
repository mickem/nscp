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