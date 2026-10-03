// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/mutex.hpp>
#include <chrono>
#include <list>
#include <memory>
#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/logger_impl.hpp>
#include <string>
#include <threads/in_flight.hpp>
#include <vector>

namespace nsclient {
namespace logging {
namespace impl {
class nsclient_logger : public logger_impl, public logging_subscriber {
  typedef std::vector<logging_subscriber_instance> subscribers_type;
  typedef std::shared_ptr<const subscribers_type> subscribers_ptr;

  log_driver_instance backend_;
  // Copy-on-write: add() / remove() / clear() publish a new vector, and a
  // delivery holds the one it started with. The list changes on module
  // load and unload only, so a log line costs one shared_ptr copy rather
  // than a node per subscriber.
  subscribers_ptr subscribers_;
  // Guards subscribers_ and the ordering between a delivery entering
  // deliveries_ and a removal taking its cutoff. Held for those few lines
  // only, never across a subscriber's on_log_message, so a plain blocking
  // mutex is safe. It used to be a 5 s timed mutex held across the whole
  // fan-out: a subscriber that logged from inside its handler re-entered on
  // the same thread, waited the 5 s and lost the line, and a remove()
  // arriving during a slow delivery gave up after 5 s and left the plugin's
  // shared_ptr in the list - the static-destruction hazard
  // plugin_manager.hpp documents.
  mutable boost::mutex mutex_;
  // Serialises the fan-out across threads. The console backend delivers on
  // whichever thread logged, and the handlers were written against the
  // one-at-a-time delivery the old mutex gave them: WEBServer numbers each
  // line before it takes its own lock, and the web UI's live log skips a
  // lower number that arrives after a higher one. Timed, as before: a line
  // that cannot get in within 5 s because a handler is stuck is dropped
  // from the fan-out (it has already reached the console or file), rather
  // than queued behind the stuck one.
  boost::timed_mutex delivery_mutex_;
  // The threads inside a delivery, so remove() / clear() can wait for the
  // ones that may still hold the subscriber they just took off the list.
  threads::in_flight deliveries_;

 public:
  nsclient_logger();
  ~nsclient_logger() override;

  void add(const logging_subscriber_instance &subscriber) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    std::shared_ptr<subscribers_type> next = subscribers_ ? std::make_shared<subscribers_type>(*subscribers_) : std::make_shared<subscribers_type>();
    next->push_back(subscriber);
    subscribers_ = next;
  }
  void clear() {
    replace_and_wait([](const subscribers_type &) { return std::make_shared<subscribers_type>(); });
  }
  void remove(const logging_subscriber_instance &subscriber) {
    replace_and_wait([&subscriber](const subscribers_type &current) {
      std::shared_ptr<subscribers_type> next = std::make_shared<subscribers_type>();
      for (const logging_subscriber_instance &s : current) {
        if (s != subscriber) next->push_back(s);
      }
      return next;
    });
  }

  // Deliver to the subscriber list as it was when the line arrived, with
  // mutex_ released: a subscriber is module code (handleMessage), and it may
  // unload modules - itself included - from inside its handler. The copy
  // keeps every subscriber alive for this delivery even if it is removed
  // meanwhile; remove() then waits for the delivery before returning.
  //
  // A thread already inside a delivery does not fan out again. On the
  // console backend a handler's own log line arrives here synchronously on
  // the same thread, and a handler that logged once per line it was handed
  // would otherwise recurse until the stack ran out (the old mutex stopped
  // that after a 5 s stall per line). The nested line has already reached
  // the console or file; only the handlers do not see it, which is what the
  // generated module glue has always promised them ("loggers cant log").
  void on_log_message(const std::string &data) override {
    if (deliveries_.on_this_thread()) return;
    boost::unique_lock<boost::timed_mutex> serial(delivery_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!serial.owns_lock()) return;
    // Declared before the snapshot so that the snapshot - and with it the
    // last reference a removed subscriber may have - is released before the
    // guard wakes a waiting remove().
    threads::in_flight::guard delivering(deliveries_);
    subscribers_ptr snapshot;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      if (!subscribers_ || subscribers_->empty()) return;
      snapshot = subscribers_;
      delivering.enter();
    }
    for (const logging_subscriber_instance &s : *snapshot) {
      s->on_log_message(data);
    }
    snapshot.reset();
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

 private:
  // Publish the list `rebuild` returns in place of the current one, then
  // wait for the deliveries that started on the old list to finish, so the
  // caller can tear the removed subscriber down. The old list is released
  // only after mutex_ is: if it held the last reference to a plugin, the
  // destructor unmaps the library, whose static destructors may log - and a
  // log line takes mutex_. The wait excludes this thread's own delivery (a
  // handler unsubscribing itself) and is bounded like dll_plugin's wait for
  // its dispatchers: a handler that has been running for five seconds is
  // not going to finish because we keep waiting.
  template <class Rebuild>
  void replace_and_wait(Rebuild rebuild) {
    subscribers_ptr previous;
    std::uint64_t cutoff = 0;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      previous = subscribers_;
      if (previous) subscribers_ = rebuild(*previous);
      cutoff = deliveries_.cutoff();
    }
    deliveries_.wait_for_others_before(cutoff, std::chrono::seconds(5));
    previous.reset();
  }
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
