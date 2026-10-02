// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <list>
#include <memory>
#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/logger_impl.hpp>
#include <set>
#include <string>

namespace nsclient {
namespace logging {
namespace impl {
class nsclient_logger : public logger_impl, public logging_subscriber {
  typedef std::list<logging_subscriber_instance> subscribers_type;

  log_driver_instance backend_;
  subscribers_type subscribers_;
  // Guards subscribers_ and the delivery bookkeeping below. It is held for
  // the list operations only, never across a subscriber's on_log_message, so
  // a plain blocking mutex is safe here. It used to be a 5 s timed mutex
  // held across the whole fan-out: a subscriber that logged from inside its
  // handler re-entered on the same thread, waited the 5 s and lost the line,
  // and a remove() arriving during a slow delivery gave up after 5 s and
  // left the plugin's shared_ptr in the list - the static-destruction hazard
  // plugin_manager.hpp documents.
  mutable boost::mutex mutex_;
  // The threads currently inside a delivery, one entry per nested delivery,
  // and the condition remove() / clear() wait on for them to finish.
  mutable std::multiset<boost::thread::id> dispatchers_;
  mutable boost::condition_variable idle_;

  // Marks this thread as delivering for as long as it lives.
  class delivery_guard {
    const nsclient_logger &owner_;

   public:
    explicit delivery_guard(const nsclient_logger &owner) : owner_(owner) {}
    ~delivery_guard() {
      {
        boost::lock_guard<boost::mutex> lock(owner_.mutex_);
        // One entry, not every entry for this thread: a nested delivery
        // leaves the outer one still running.
        const std::multiset<boost::thread::id>::iterator it = owner_.dispatchers_.find(boost::this_thread::get_id());
        if (it != owner_.dispatchers_.end()) owner_.dispatchers_.erase(it);
      }
      owner_.idle_.notify_all();
    }
    delivery_guard(const delivery_guard &) = delete;
    delivery_guard &operator=(const delivery_guard &) = delete;
  };

  // With mutex_ held: wait until no other thread is inside a delivery that
  // may still be calling a subscriber this thread just took off the list,
  // so the caller can tear it down afterwards. This thread's own deliveries
  // are not waited for - a log-handler module that unloads a module from
  // inside its handler arrives here from one, and it only ends when this
  // returns. Bounded like dll_plugin's wait for its dispatchers: a handler
  // that has been running for five seconds is not going to finish because
  // we keep waiting, and an unbounded wait would stall the unload behind it.
  void wait_for_other_deliveries(boost::unique_lock<boost::mutex> &lock) const {
    const boost::thread::id self = boost::this_thread::get_id();
    const boost::system_time deadline = boost::get_system_time() + boost::posix_time::seconds(5);
    while (dispatchers_.size() != dispatchers_.count(self)) {
      if (!idle_.timed_wait(lock, deadline)) return;
    }
  }

 public:
  nsclient_logger();
  ~nsclient_logger() override;

  void add(const logging_subscriber_instance &subscriber) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    subscribers_.push_back(subscriber);
  }
  void clear() {
    boost::unique_lock<boost::mutex> lock(mutex_);
    subscribers_.clear();
    wait_for_other_deliveries(lock);
  }
  void remove(const logging_subscriber_instance &subscriber) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    subscribers_.remove(subscriber);
    wait_for_other_deliveries(lock);
  }

  // Deliver to a snapshot of the list taken under the lock, with the lock
  // released: a subscriber is module code (handleMessage), and it may log
  // again, or load and unload modules, from inside its handler. The copies
  // keep each subscriber alive for this delivery even if it is removed
  // meanwhile; remove() then waits for the delivery before returning.
  void on_log_message(const std::string &data) override {
    subscribers_type snapshot;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      if (subscribers_.empty()) return;
      snapshot = subscribers_;
      dispatchers_.insert(boost::this_thread::get_id());
    }
    const delivery_guard delivering(*this);
    for (logging_subscriber_instance &s : snapshot) {
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
