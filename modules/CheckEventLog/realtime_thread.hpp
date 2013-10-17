#pragma once

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include "eventlog_wrapper.hpp"
#include "eventlog_record.hpp"
#include "filter_config_object.hpp"

struct real_time_thread {
	bool enabled_;
	unsigned long long start_age_;
	boost::shared_ptr<boost::thread> thread_;
	HANDLE stop_event_;
	eventlog_filter::filter_config_handler filters_;
	std::string logs_;

	bool cache_;
	bool debug_;
	std::string filters_path_;

	real_time_thread() : enabled_(false), start_age_(0), debug_(false), cache_(false) {
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
	bool start();
	bool stop();

	void thread_proc();
	//	void process_events(eventlog_filter::filter_engine engine, eventlog_wrapper &eventlog);
	void process_no_events(const eventlog_filter::filter_config_object &object);
	void process_record(eventlog_filter::filter_config_object &object, const EventLogRecord &record);
	void debug_miss(const EventLogRecord &record);
	//	void process_event(eventlog_filter::filter_engine engine, const EVENTLOGRECORD* record);
};
