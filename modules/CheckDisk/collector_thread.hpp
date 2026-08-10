// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <map>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>
#include <trend/trend_buffer.hpp>

#include "check_disk_io.hpp"

class collector_thread {
 public:
  typedef std::map<std::string, trend::trend_buffer> trend_map;

 private:
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

  // Used-bytes history per drive (keyed like disk_free_: mountpoint on Unix,
  // "C:" on Windows), feeding check_drivesize's full_in/rate keywords. Fed
  // from the same fetch as disk_free_; the buffers subsample to trend_interval.
  boost::mutex trends_mutex_;
  trend_map trends_;

 public:
  int collection_interval;
  std::string disable_;
  // Trend cadence/retention in seconds (settings `trend interval` /
  // `trend retention` under the disk alias); set before start().
  long long trend_interval;
  long long trend_retention;

  collector_thread(nscapi::core_wrapper *core, const int plugin_id)
      : stop_requested_(false),
        plugin_id_(plugin_id),
        core_(core),
        collection_interval(10),
        trend_interval(300),
        trend_retention(7 * 24 * 3600) {}

  disk_io_check::disks_type get_disk_io();
  disk_free_check::drives_type get_disk_free();
  trend_map get_drive_trends();

  bool start();
  bool stop();

  static std::string to_string() { return "disk_collector"; }

 private:
  void thread_proc();
  void update_trends(long long now);
  // Persistence via the core storage API (context "disk.trends", one key per
  // drive): loaded on start, saved hourly and on clean stop. The stored form
  // is downsampled to 30-minute granularity to keep nsclient.db small.
  void load_trends();
  void save_trends();
};
