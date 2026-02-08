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

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <nsclient/logger/logger_impl.hpp>
#include <nsclient/logger/logger.hpp>
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

  void add(const logging_subscriber_instance& subscriber) {
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