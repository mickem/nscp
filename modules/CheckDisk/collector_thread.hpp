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

#pragma once

#include <boost/thread/thread.hpp>
#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>

#include "check_disk_io.hpp"

class collector_thread {
  std::shared_ptr<boost::thread> thread_;
  HANDLE stop_event_;
  int plugin_id_;
  nscapi::core_wrapper *core_;

  disk_io_check::disk_io_data disk_io_;
  disk_free_check::disk_free_data disk_free_;

 public:
  int collection_interval;
  std::string disable_;

  collector_thread(nscapi::core_wrapper *core, const int plugin_id) : stop_event_(nullptr), plugin_id_(plugin_id), core_(core), collection_interval(10) {}

  disk_io_check::disks_type get_disk_io();
  disk_free_check::drives_type get_disk_free();

  bool start();
  bool stop();

  static std::string to_string() { return "disk_collector"; }

 private:
  void thread_proc();
};
