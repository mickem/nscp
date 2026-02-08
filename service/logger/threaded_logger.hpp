/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <boost/thread.hpp>
#include <concurrent_queue.hpp>
#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <string>

namespace nsclient {
namespace logging {
namespace impl {
class threaded_logger : public log_driver_interface_impl {
  concurrent_queue<std::string> log_queue_;
  boost::thread thread_;

  logging_subscriber *subscriber_manager_;
  log_driver_instance background_logger_;

 public:
  threaded_logger(logging_subscriber *subscriber_manager, log_driver_instance background_logger);
  ~threaded_logger() override;

  void do_log(std::string data) override;
  void push(const std::string &data);

  void thread_proc();

  void asynch_configure() override;
  void synch_configure() override;
  bool startup() override;
  bool shutdown() override;

  void set_config(const std::string &key) override;
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient