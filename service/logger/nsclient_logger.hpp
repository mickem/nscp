// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <memory>
#include <nsclient/logger/log_driver_interface_impl.hpp>
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

  // The three mutators block rather than giving up after five seconds.
  //
  // remove()/clear() are what remove_plugin and purge_broken_plugin use to
  // drop a module's log subscription, and a timeout there left a destroyed
  // plugin's shared_ptr in the list - the static-destruction hazard
  // plugin_manager.hpp documents. The critical section is now a list
  // operation and nothing else (see on_log_message), so there is nothing left
  // to wait five seconds for.
  void add(const logging_subscriber_instance &subscriber) {
    boost::lock_guard<boost::timed_mutex> lock(mutex_);
    subscribers_.push_back(subscriber);
  }
  void clear() {
    boost::lock_guard<boost::timed_mutex> lock(mutex_);
    subscribers_.clear();
  }
  void remove(const logging_subscriber_instance &subscriber) {
    boost::lock_guard<boost::timed_mutex> lock(mutex_);
    subscribers_.remove(subscriber);
  }

  void on_log_message(const std::string &data) override {
    // Snapshot under the lock, dispatch outside it.
    //
    // With the console backend - the default for `nscp test` and
    // `nscp client` - subscribers are called synchronously on the logging
    // thread, so a log-handler module that logs from inside its own handler
    // arrived back here on the thread already holding this mutex: it waited
    // out the five seconds and then dropped the line. The copy also keeps each
    // subscriber alive for its call, which is what lets remove() block without
    // the two deadlocking.
    std::list<logging_subscriber_instance> subscribers;
    {
      boost::lock_guard<boost::timed_mutex> lock(mutex_);
      if (subscribers_.empty()) return;
      subscribers = subscribers_;
    }
    for (logging_subscriber_instance &s : subscribers) {
      s->on_log_message(data);
    }
  }

  // Takes both severity names ("debug", "trace", ...) and log-driver options
  // ("console", "no-console", "oneline", "no-std-err") - cli_parser pushes
  // both onto the same list. Only "console" used to be routed to the backend,
  // so --no-stderr and oneline reached log_level::set(), which does not know
  // them, and logged "Invalid log level: no-std-err" instead of taking effect.
  void set_log_level(const std::string level) override {
    if (log_driver_interface_impl::is_driver_option(level)) {
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
  void remove_subscriber(logging_subscriber_instance subscriber) override;
  void clear_subscribers() override;
  bool startup() override;
  bool shutdown() override;
  void configure() override;
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
