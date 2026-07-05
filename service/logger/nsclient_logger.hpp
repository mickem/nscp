// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/mutex.hpp>
#include <list>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/logger_impl.hpp>
#include <string>

namespace nsclient {
namespace logging {
namespace impl {
class nsclient_logger : public logger_impl, public logging_subscriber {
  typedef std::list<logging_subscriber_instance> subscribers_type;

  log_driver_instance backend_;
  subscribers_type subscribers_;
  mutable boost::timed_mutex mutex_;

 public:
  nsclient_logger();
  ~nsclient_logger() override;

  void add(const logging_subscriber_instance &subscriber) {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return;
    subscribers_.push_back(subscriber);
  }
  void clear() {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return;
    subscribers_.clear();
  }

  void on_log_message(const std::string &data) override {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return;
    if (subscribers_.empty()) return;
    for (logging_subscriber_instance &s : subscribers_) {
      s->on_log_message(data);
    }
  }

  void set_log_level(const std::string level) override {
    if (level == "console") {
      if (backend_) {
        backend_->set_config(level);
      }
    } else {
      logger_impl::set_log_level(level);
    }
  }

  void do_log(std::string data) override;

  void set_backend(std::string backend) override;
  void destroy() override;

  void add_subscriber(logging_subscriber_instance) override;
  void clear_subscribers() override;
  bool startup() override;
  bool shutdown() override;
  void configure() override;
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
