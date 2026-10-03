// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
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
  // Mirrors "subscribers_ is non-empty", maintained under mutex_, so the
  // common case - no log-handler module loaded, as in every CLI mode - costs
  // a log line no lock at all.
  std::atomic<bool> has_subscribers_{false};
  // Guards subscribers_, handler_lines_ and the ordering between a delivery
  // entering deliveries_ and a removal taking its cutoff. Held for those few
  // lines only, never across a subscriber's on_log_message, so a plain
  // blocking mutex is safe. It used to be a 5 s timed mutex held across the
  // whole fan-out: a subscriber that logged from inside its handler
  // re-entered on the same thread, waited the 5 s and lost the line, a
  // remove() arriving during a slow delivery gave up after 5 s and left the
  // plugin's shared_ptr in the list - the static-destruction hazard
  // plugin_manager.hpp documents - and one stuck handler cost every other
  // thread 5 s per line. Handlers run concurrently now, on the console
  // backend; the threaded backend still delivers from its one worker.
  mutable boost::mutex mutex_;
  // The threads inside a delivery, so remove() / clear() can wait for the
  // ones that may still hold the subscriber they just took off the list.
  threads::in_flight deliveries_;
  // How long a removal waits for the deliveries in flight: 5 s in the
  // service, like dll_plugin's wait for its dispatchers. Settable so a test
  // can see the timed-out path in less.
  std::chrono::milliseconds delivery_wait_{5000};

  // Lines a handler logged from inside a delivery, waiting to come back
  // through on_log_message. Such a line is not fanned out to the handlers
  // again: a handler that logged once per line it was handed would
  // otherwise feed itself forever - as a stack overflow on the console
  // backend, where its line arrives here synchronously on the same thread,
  // and as a queue that never drains on the threaded backend, where the
  // worker delivers the line it queued next. The line has already reached
  // the console or file by then; only the handlers do not see it, which is
  // what the generated module glue has always promised them ("loggers cant
  // log"). Tagging the line here, at do_log, is what covers both backends:
  // a check on the delivering thread's depth alone sees only the
  // synchronous one. Guarded by mutex_; the counter lets the delivery path
  // skip the lock while nothing is pending, which is almost always.
  std::deque<std::string> handler_lines_;
  std::atomic<unsigned> handler_lines_pending_{0};
  // A backend that drops lines (a full queue) would leave their tags behind
  // for good, so the oldest goes when this many are waiting.
  static const std::size_t max_handler_lines_ = 1024;

  // How many deliveries the calling thread is inside: a line logged while it
  // is above zero was produced by a handler. Thread-local so do_log can tell
  // without a lock; the process has one nsclient_logger, so one counter per
  // thread is enough.
  static unsigned &delivery_depth() {
    static thread_local unsigned depth = 0;
    return depth;
  }
  struct depth_guard {
    depth_guard() { ++delivery_depth(); }
    ~depth_guard() { --delivery_depth(); }
    depth_guard(const depth_guard &) = delete;
    depth_guard &operator=(const depth_guard &) = delete;
  };

 public:
  nsclient_logger();
  ~nsclient_logger() override;

  void add(const logging_subscriber_instance &subscriber) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    std::shared_ptr<subscribers_type> next = subscribers_ ? std::make_shared<subscribers_type>(*subscribers_) : std::make_shared<subscribers_type>();
    next->push_back(subscriber);
    subscribers_ = next;
    has_subscribers_ = true;
  }
  // For tests: how long the bounded wait runs (see delivery_wait_).
  void set_delivery_wait(std::chrono::milliseconds wait) { delivery_wait_ = wait; }
  // For tests: install a backend built by the caller, as set_backend(name)
  // would install one of its own.
  void use_backend(log_driver_instance backend);

  // Take every subscriber off the list and wait for the deliveries that
  // started on the old list to finish. Nothing in flight can hold what was
  // never on the list, so an empty list returns at once.
  unsubscribe_result clear() {
    unsubscribe_result result;
    subscribers_ptr previous;
    std::uint64_t cutoff = 0;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      if (!subscribers_ || subscribers_->empty()) return result;
      result.removed = true;
      previous = subscribers_;
      subscribers_ = std::make_shared<subscribers_type>();
      has_subscribers_ = false;
      cutoff = deliveries_.cutoff();
    }
    result.delivering = !wait_and_release(previous, cutoff);
    return result;
  }
  // Take one subscriber off the list and wait for the deliveries that
  // started on the old list, so the caller can tear it down afterwards. A
  // subscriber that was not on the list is not waited for either: the
  // plugin manager unsubscribes every module it unloads, handler or not,
  // and that must not park an unload behind an unrelated handler.
  unsubscribe_result remove(const logging_subscriber_instance &subscriber) {
    unsubscribe_result result;
    subscribers_ptr previous;
    std::uint64_t cutoff = 0;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      if (!subscribers_) return result;
      std::shared_ptr<subscribers_type> next = std::make_shared<subscribers_type>();
      for (const logging_subscriber_instance &s : *subscribers_) {
        if (s == subscriber)
          result.removed = true;
        else
          next->push_back(s);
      }
      if (!result.removed) return result;
      previous = subscribers_;
      subscribers_ = next;
      has_subscribers_ = !next->empty();
      cutoff = deliveries_.cutoff();
    }
    result.delivering = !wait_and_release(previous, cutoff);
    return result;
  }

  // Deliver to the subscriber list as it was when the line arrived, with
  // mutex_ released: a subscriber is module code (handleMessage), and it may
  // unload modules - itself included - from inside its handler. The copy
  // keeps every subscriber alive for this delivery even if it is removed
  // meanwhile; remove() then waits for the delivery before returning. A
  // line a handler logged (see handler_lines_) is not delivered at all.
  void on_log_message(const std::string &data) override {
    if (handler_lines_pending_.load(std::memory_order_acquire) > 0 && consume_handler_line(data)) return;
    if (!has_subscribers_.load(std::memory_order_acquire)) return;
    // Both guards are declared before the snapshot, so that the snapshot -
    // and with it the last reference a removed subscriber may have - is
    // released while this thread still counts as delivering and before the
    // tracker wakes a waiting remove(). A plugin destroyed by that release
    // may log from its teardown; that line is then a handler line too.
    threads::in_flight::guard delivering(deliveries_);
    const depth_guard nested;
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
  unsubscribe_result remove_subscriber(logging_subscriber_instance subscriber) override;
  unsubscribe_result clear_subscribers() override;
  bool startup() override;
  bool shutdown() override;
  void configure() override;

 private:
  // With the list already replaced: wait for the deliveries that started on
  // `previous` to finish, then let it go. The old list is released only
  // after mutex_ is (the caller dropped the lock before calling this): if it
  // held the last reference to a plugin, the destructor unmaps the library,
  // whose static destructors may log - and a log line takes mutex_. The
  // wait excludes this thread's own delivery (a handler unsubscribing
  // itself) and is bounded like dll_plugin's wait for its dispatchers: a
  // handler that has been running for five seconds is not going to finish
  // because we keep waiting. Returns false when it ran out.
  bool wait_and_release(subscribers_ptr &previous, std::uint64_t cutoff) {
    const bool clean = deliveries_.wait_for_others_before(cutoff, delivery_wait_);
    previous.reset();
    return clean;
  }

  // Called from do_log on a thread inside a delivery: the line is a
  // handler's, and must not be fanned out when it comes back.
  void remember_handler_line(const std::string &data) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    if (handler_lines_.size() >= max_handler_lines_) {
      handler_lines_.pop_front();
      handler_lines_pending_.fetch_sub(1, std::memory_order_acq_rel);
    }
    handler_lines_.push_back(data);
    handler_lines_pending_.fetch_add(1, std::memory_order_acq_rel);
  }
  // Whether `data` is a remembered handler line; forgets it if so.
  bool consume_handler_line(const std::string &data) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    for (std::deque<std::string>::iterator it = handler_lines_.begin(); it != handler_lines_.end(); ++it) {
      if (*it == data) {
        handler_lines_.erase(it);
        handler_lines_pending_.fetch_sub(1, std::memory_order_acq_rel);
        return true;
      }
    }
    return false;
  }
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
