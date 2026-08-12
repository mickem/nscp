// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <map>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <set>
#include <string>
#include <trend/trend_buffer.hpp>

#include "check_disk_io.hpp"

// Consecutive-failure tracker for a single collection.
//
// A collection is only given up on after `limit` failures in a row; a single
// failed fetch is not evidence that the data source is unavailable (WMI is
// routinely busy for a tick), so the collector keeps retrying until the
// failures persist. A limit of 0 or less never gives up. Any success resets
// the count, so intermittent errors never accumulate into a give-up.
class collector_failure_tracker {
  int limit_;
  int consecutive_;
  bool given_up_;

 public:
  explicit collector_failure_tracker(const int limit = 0) : limit_(limit), consecutive_(0), given_up_(false) {}

  bool given_up() const { return given_up_; }
  int consecutive() const { return consecutive_; }
  int limit() const { return limit_; }

  void succeeded() { consecutive_ = 0; }
  // Records a failure; returns true when this was the failure that gave up
  // (so the caller can log the transition exactly once).
  bool failed() {
    if (given_up_) return false;
    ++consecutive_;
    if (limit_ > 0 && consecutive_ >= limit_) {
      given_up_ = true;
      return true;
    }
    return false;
  }
};

class collector_thread {
 public:
  typedef std::map<std::string, trend::trend_buffer> trend_map;
  // Immutable snapshot handed to checks: shared, never copied per check.
  typedef std::shared_ptr<const trend_map> trend_snapshot;

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
  // Copy-on-publish view of trends_, rebuilt once per collector tick.
  trend_snapshot snapshot_;
  // Storage keys written by save_trends(), so rows for drives that have since
  // aged out can be cleared instead of lingering in nsclient.db forever.
  std::set<std::string> saved_keys_;

 public:
  int collection_interval;
  std::string disable_;
  // Trend cadence/retention in seconds (settings `trend interval` /
  // `trend retention` under the disk alias); set before start().
  long long trend_interval;
  long long trend_retention;
  // How many consecutive failed fetches disable a collection for the rest of
  // the process lifetime (setting `max collection errors`; 0 = never give up).
  // A single failure is not evidence that the source is gone - WMI is
  // routinely busy for a tick - so the collector retries and only stops once
  // the failures persist. Success resets the count.
  int max_collection_errors;

  collector_thread(nscapi::core_wrapper *core, const int plugin_id)
      : stop_requested_(false),
        plugin_id_(plugin_id),
        core_(core),
        collection_interval(10),
        trend_interval(300),
        trend_retention(7 * 24 * 3600),
        max_collection_errors(10) {}

  disk_io_check::disks_type get_disk_io();
  disk_free_check::drives_type get_disk_free();
  // May be null before the first collector tick has published anything.
  trend_snapshot get_drive_trends();

  bool start();
  bool stop();

  static std::string to_string() { return "disk_collector"; }

 private:
  void thread_proc();
  void update_trends(long long now);
  // Rebuild snapshot_ from trends_; must be called with trends_mutex_ held.
  void publish_trends();
  // Persistence via the core storage API (context "disk.trends", one key per
  // drive): loaded on start, saved hourly and on clean stop. The stored form
  // is downsampled to 30-minute granularity to keep nsclient.db small.
  void load_trends();
  void save_trends();
};
