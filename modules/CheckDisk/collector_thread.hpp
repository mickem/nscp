// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>

#include "check_disk_io.hpp"

class collector_thread {
  std::shared_ptr<boost::thread> thread_;
  // Portable stop signalling (replaces the Win32 event so the collector is
  // cross-platform): set the flag under the mutex and notify the CV; the
  // worker waits on the CV with a timeout equal to the collection interval.
  boost::mutex stop_mutex_;
  boost::condition_variable stop_cv_;
  bool stop_requested_;
  int plugin_id_;
  nscapi::core_wrapper *core_;

  disk_io_check::disk_io_data disk_io_;
  disk_free_check::disk_free_data disk_free_;

 public:
  int collection_interval;
  std::string disable_;

  collector_thread(nscapi::core_wrapper *core, const int plugin_id) : stop_requested_(false), plugin_id_(plugin_id), core_(core), collection_interval(10) {}

  disk_io_check::disks_type get_disk_io();
  disk_free_check::drives_type get_disk_free();

  bool start();
  bool stop();

  static std::string to_string() { return "disk_collector"; }

 private:
  void thread_proc();
};
