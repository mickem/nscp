// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collector_thread.hpp"

#include <ctime>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nsclient/nsclient_exception.hpp>

namespace {
// Granularity of the persisted form: coarse enough to keep nsclient.db small,
// fine enough that a long-window trend survives a restart usefully. The
// in-memory 5-minute resolution rebuilds within the hour after a restart.
const long long trend_save_granularity = 30 * 60;
const long long trend_save_interval = 3600;
}  // namespace

disk_io_check::disks_type collector_thread::get_disk_io() { return disk_io_.get(); }

disk_free_check::drives_type collector_thread::get_disk_free() { return disk_free_.get(); }

collector_thread::trend_map collector_thread::get_drive_trends() {
  const boost::lock_guard<boost::mutex> lock(trends_mutex_);
  return trends_;
}

bool collector_thread::start() {
  {
    const boost::lock_guard<boost::mutex> lock(stop_mutex_);
    stop_requested_ = false;
  }
  thread_ = std::make_shared<boost::thread>([this]() { this->thread_proc(); });
  return true;
}

bool collector_thread::stop() {
  {
    const boost::lock_guard<boost::mutex> lock(stop_mutex_);
    stop_requested_ = true;
  }
  stop_cv_.notify_all();
  if (thread_) {
    thread_->join();
    thread_.reset();
  }
  return true;
}

void collector_thread::update_trends(const long long now) {
  const disk_free_check::drives_type drives = disk_free_.get();
  const boost::lock_guard<boost::mutex> lock(trends_mutex_);
  for (const disk_free_check::disk_free &d : drives) {
    trend_map::iterator it = trends_.find(d.name);
    if (it == trends_.end()) it = trends_.insert(trend_map::value_type(d.name, trend::trend_buffer(trend_interval, trend_retention))).first;
    // Track used bytes; the total is the reset context (a resize invalidates
    // the history).
    it->second.append(now, d.total - d.free, d.total);
  }
  // Drives that have come and gone age out with their samples.
  for (trend_map::iterator it = trends_.begin(); it != trends_.end();) {
    if (it->second.empty() || it->second.newest_ts() < now - trend_retention)
      it = trends_.erase(it);
    else
      ++it;
  }
}

void collector_thread::load_trends() {
  try {
    nscapi::core_helper core(core_, plugin_id_);
    const long long now = static_cast<long long>(std::time(nullptr));
    trend_map loaded;
    for (const nscapi::core_helper::storage_map::value_type &e : core.get_storage_strings("disk.trends")) {
      trend::trend_buffer buf = trend::trend_buffer::decode(e.second, trend_interval, trend_retention, now);
      if (!buf.empty()) loaded[e.first] = buf;
    }
    if (loaded.empty()) return;
    const boost::lock_guard<boost::mutex> lock(trends_mutex_);
    trends_.swap(loaded);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to load drive trends: " + std::string(e.what()));
  } catch (...) {
    NSC_LOG_ERROR("Failed to load drive trends");
  }
}

void collector_thread::save_trends() {
  try {
    trend_map snapshot = get_drive_trends();
    nscapi::core_helper core(core_, plugin_id_);
    for (const trend_map::value_type &v : snapshot) {
      if (v.second.empty()) continue;
      core.put_storage("disk.trends", v.first, v.second.encode(trend_save_granularity), false, false);
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to save drive trends: " + std::string(e.what()));
  } catch (...) {
    NSC_LOG_ERROR("Failed to save drive trends");
  }
}

void collector_thread::thread_proc() {
  bool disable_disk_io = disable_.find("disk_io") != std::string::npos;
  if (disable_disk_io) {
    NSC_LOG_MESSAGE("WARNING: disk I/O checking is disabled");
  }
  bool disable_disk_free = disable_.find("disk_free") != std::string::npos;
  if (disable_disk_free) {
    NSC_LOG_MESSAGE("WARNING: disk free checking is disabled");
  }
  // Trends ride on the disk_free fetch, so disabling that disables them too.
  bool disable_trend = disable_disk_free || disable_.find("trend") != std::string::npos;
  if (disable_trend && !disable_disk_free) {
    NSC_LOG_MESSAGE("WARNING: disk trend tracking is disabled");
  }

  if (!disable_trend) load_trends();

  // Initial fetch to populate data immediately.
  if (!disable_disk_io) {
    try {
      disk_io_.fetch();
    } catch (const nsclient::nsclient_exception &e) {
      NSC_LOG_ERROR("Initial disk I/O fetch failed: " + e.reason());
    } catch (...) {
      NSC_LOG_ERROR("Initial disk I/O fetch failed");
      disable_disk_io = false;
    }
  }
  if (!disable_disk_free) {
    try {
      disk_free_.fetch();
    } catch (...) {
      NSC_LOG_ERROR("Initial disk free fetch failed");
      disable_disk_free = true;
    }
  }

  const long long started = static_cast<long long>(std::time(nullptr));
  long long last_save = started;
  for (;;) {
    if (!disable_disk_io) {
      try {
        disk_io_.fetch();
      } catch (const nsclient::nsclient_exception &e) {
        NSC_LOG_ERROR("Failed to get disk I/O metrics: " + e.reason());
      } catch (const std::exception &e) {
        NSC_LOG_ERROR("Failed to get disk I/O metrics: " + std::string(e.what()));
      } catch (...) {
        NSC_LOG_ERROR("Failed to get disk I/O metrics");
      }
    }
    if (!disable_disk_free) {
      try {
        disk_free_.fetch();
      } catch (const std::exception &e) {
        NSC_LOG_ERROR("Failed to get disk free metrics: " + std::string(e.what()));
      } catch (...) {
        NSC_LOG_ERROR("Failed to get disk free metrics");
      }
      if (!disable_trend) {
        const long long now = static_cast<long long>(std::time(nullptr));
        update_trends(now);
        if (now - last_save >= trend_save_interval) {
          save_trends();
          last_save = now;
        }
      }
    }
    // Sleep until the next interval, waking early if stop() was requested.
    boost::unique_lock<boost::mutex> lock(stop_mutex_);
    if (stop_cv_.timed_wait(lock, boost::posix_time::seconds(collection_interval), [this]() { return stop_requested_; })) {
      break;  // stop requested
    }
  }
  // Skip the shutdown save when the collector ran for less than one trend
  // interval: nothing worth persisting can have accumulated, and one-shot
  // client runs (nscp client --boot) would otherwise attempt (and, without a
  // writable data path, noisily fail) a storage write on every invocation.
  if (!disable_trend && static_cast<long long>(std::time(nullptr)) - started >= trend_interval) save_trends();
}
