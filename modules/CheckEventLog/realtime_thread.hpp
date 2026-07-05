// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/thread.hpp>
#include <memory>

#include "eventlog_record.hpp"
#include "eventlog_wrapper.hpp"
#include "filter_config_object.hpp"

struct real_time_thread {
  nscapi::core_wrapper *core;
  int plugin_id;
  bool enabled_;
  unsigned long long start_age_;
  std::shared_ptr<boost::thread> thread_;
  HANDLE stop_event_;
  eventlog_filter::filter_config_handler filters_;
  std::string logs_;

  bool cache_;
  bool debug_;

  real_time_thread(nscapi::core_wrapper *core, int plugin_id) : core(core), plugin_id(plugin_id), enabled_(false), start_age_(0), debug_(false), cache_(false) {
    set_start_age("30m");
  }

  void add_realtime_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
  void set_enabled(bool flag) { enabled_ = flag; }
  void set_start_age(std::string age) { start_age_ = str::format::stox_as_time_sec<unsigned long long>(age, "s"); }

  void set_language(std::string lang);
  void set_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string flt) {
    if (!flt.empty()) add_realtime_filter(proxy, "default", flt);
  }
  bool has_filters() { return !filters_.has_objects(); }

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
