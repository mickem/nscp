// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>

#include "filter_config_object.hpp"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#endif

struct real_time_thread {
  std::shared_ptr<boost::thread> thread_;
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

  void add_realtime_filter(nscapi::settings_helper::settings_impl_interface_ptr proxy, std::string key, std::string query);
  void ensure_default(nscapi::settings_helper::settings_impl_interface_ptr proxy);
  void set_enabled(bool flag) { enabled_ = flag; }

  void set_language(std::string lang);
  void set_filter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string flt) {
    if (!flt.empty()) add_realtime_filter(proxy, "default", flt);
  }
  bool has_filters() { return !filters_.has_objects(); }
  bool start();
  bool stop();

  void thread_proc();
  void process_object(filters::filter_config_object &object);
  void process_timeout(const filters::filter_config_object &object);
};