// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>
#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <string>
#include <threads/concurrent_queue.hpp>

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