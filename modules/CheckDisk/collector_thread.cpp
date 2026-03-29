/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "collector_thread.hpp"

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nsclient/nsclient_exception.hpp>

disk_io_check::disks_type collector_thread::get_disk_io() { return disk_io_.get(); }

disk_free_check::drives_type collector_thread::get_disk_free() { return disk_free_.get(); }

bool collector_thread::start() {
  stop_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  thread_ = std::make_shared<boost::thread>([this]() { this->thread_proc(); });
  return true;
}

bool collector_thread::stop() {
  if (stop_event_ != nullptr) {
    SetEvent(stop_event_);
    if (thread_) thread_->join();
    CloseHandle(stop_event_);
    stop_event_ = nullptr;
  }
  return true;
}

void collector_thread::thread_proc() {
  const bool disable_disk_io = disable_.find("disk_io") != std::string::npos;
  if (disable_disk_io) {
    NSC_LOG_MESSAGE("WARNING: disk I/O checking is disabled");
  }
  const bool disable_disk_free = disable_.find("disk_free") != std::string::npos;
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
    }
  }
  if (!disable_disk_free) {
    try {
      disk_free_.fetch();
    } catch (...) {
      NSC_LOG_ERROR("Initial disk free fetch failed");
    }
  }

  DWORD wait_status = 0;
  do {
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
  } while ((wait_status = WaitForSingleObject(stop_event_, collection_interval * 1000)) == WAIT_TIMEOUT);
}
