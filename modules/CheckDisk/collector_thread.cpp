// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "collector_thread.hpp"

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nsclient/nsclient_exception.hpp>

disk_io_check::disks_type collector_thread::get_disk_io() { return disk_io_.get(); }

disk_free_check::drives_type collector_thread::get_disk_free() { return disk_free_.get(); }

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

void collector_thread::thread_proc() {
  bool disable_disk_io = disable_.find("disk_io") != std::string::npos;
  if (disable_disk_io) {
    NSC_LOG_MESSAGE("WARNING: disk I/O checking is disabled");
  }
  bool disable_disk_free = disable_.find("disk_free") != std::string::npos;
  if (disable_disk_free) {
    NSC_LOG_MESSAGE("WARNING: disk free checking is disabled");
  }

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
    }
    // Sleep until the next interval, waking early if stop() was requested.
    boost::unique_lock<boost::mutex> lock(stop_mutex_);
    if (stop_cv_.timed_wait(lock, boost::posix_time::seconds(collection_interval), [this]() { return stop_requested_; })) {
      break;  // stop requested
    }
  }
}
