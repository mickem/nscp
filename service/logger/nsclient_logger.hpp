// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <chrono>
#include <cstdint>
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
  // A subscriber and the deliveries inside it. The tracker is per
  // subscriber so that removing one waits for the lines still inside *it*,
  // not for a slow handler in an unrelated module - an unload of CheckNSCP
  // must not be refused because ElasticClient is stuck.
  struct entry {
    logging_subscriber_instance subscriber;
    std::shared_ptr<threads::in_flight> deliveries;
  };
  typedef std::vector<entry> subscribers_type;
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
  // Guards subscribers_ and the ordering between a delivery entering the
  // trackers and a removal taking its cutoffs. Held for those few lines
  // only, never across a subscriber's on_log_message, so a plain blocking
  // mutex is safe. It used to be a 5 s timed mutex held across the whole
  // fan-out: a subscriber that logged from inside its handler re-entered on
  // the same thread, waited the 5 s and lost the line, a remove() arriving
  // during a slow delivery gave up after 5 s and left the plugin's
  // shared_ptr in the list - the static-destruction hazard
  // plugin_manager.hpp documents - and one stuck handler cost every other
  // thread 5 s per line. Handlers run concurrently now, on the console
  // backend; the threaded backend still delivers from its one worker.
  mutable boost::mutex mutex_;
  // How long a removal waits for the deliveries in flight: 5 s in the
  // service, like dll_plugin's wait for its dispatchers. Settable so a test
  // can see the timed-out path in less.
  std::chrono::milliseconds delivery_wait_{5000};

  // How many deliveries the calling thread is inside: a line logged while it
  // is above zero was produced by a handler, and do_log marks it as such
  // (see tag_handler_line in the .cpp). Thread-local so do_log can tell
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
    next->push_back(entry{subscriber, std::make_shared<threads::in_flight>()});
    subscribers_ = next;
    has_subscribers_ = true;
  }
  // For tests: how long the bounded wait runs (see delivery_wait_).
  void set_delivery_wait(std::chrono::milliseconds wait) { delivery_wait_ = wait; }
  // For tests: install a backend built by the caller, as set_backend(name)
  // would install one of its own.
  void use_backend(log_driver_instance backend);

  // Take every subscriber off the list and wait for the deliveries that
  // started on the old list to finish, within one shared bound. Returns the
  // subscribers a delivery was still inside when it ran out, so the caller
  // can leave those alone; an empty list means every one is clear. Nothing
  // in flight can hold what was never on the list, so an empty list returns
  // at once.
  std::vector<logging_subscriber_instance> clear() {
    std::vector<logging_subscriber_instance> still_delivering;
    subscribers_ptr previous;
    std::vector<std::uint64_t> cutoffs;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      if (!subscribers_ || subscribers_->empty()) return still_delivering;
      previous = subscribers_;
      subscribers_ = std::make_shared<subscribers_type>();
      has_subscribers_ = false;
      cutoffs.reserve(previous->size());
      for (const entry &e : *previous) cutoffs.push_back(e.deliveries->cutoff());
    }
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + delivery_wait_;
    for (std::size_t i = 0; i < previous->size(); ++i) {
      const entry &e = (*previous)[i];
      const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
      const std::chrono::milliseconds remaining =
          now < deadline ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now) : std::chrono::milliseconds(0);
      if (!e.deliveries->wait_for_others_before(cutoffs[i], remaining)) still_delivering.push_back(e.subscriber);
    }
    // Released only after mutex_ is: if the old list held the last reference
    // to a plugin, the destructor unmaps the library, whose static
    // destructors may log - and a log line takes mutex_.
    previous.reset();
    return still_delivering;
  }
  // Take one subscriber off the list and wait for the deliveries that
  // started on the old list and are inside it, so the caller can tear it
  // down afterwards. A subscriber that was not on the list is not waited
  // for either - the plugin manager unsubscribes every module it unloads,
  // handler or not, and that must not park an unload behind an unrelated
  // handler - and costs no copy: the list is scanned first and copied only
  // on a hit.
  unsubscribe_result remove(const logging_subscriber_instance &subscriber) {
    unsubscribe_result result;
    subscribers_ptr previous;
    std::shared_ptr<threads::in_flight> deliveries;
    std::uint64_t cutoff = 0;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      if (!subscribers_) return result;
      std::size_t at = subscribers_->size();
      for (std::size_t i = 0; i < subscribers_->size(); ++i) {
        if ((*subscribers_)[i].subscriber == subscriber) {
          at = i;
          break;
        }
      }
      if (at == subscribers_->size()) return result;
      result.removed = true;
      std::shared_ptr<subscribers_type> next = std::make_shared<subscribers_type>();
      next->reserve(subscribers_->size() - 1);
      for (std::size_t i = 0; i < subscribers_->size(); ++i) {
        if (i != at) next->push_back((*subscribers_)[i]);
      }
      deliveries = (*subscribers_)[at].deliveries;
      cutoff = deliveries->cutoff();
      previous = subscribers_;
      subscribers_ = next;
      has_subscribers_ = !next->empty();
    }
    // The wait excludes this thread's own delivery (a handler unsubscribing
    // itself) and is bounded like dll_plugin's wait for its dispatchers: a
    // handler that has been running for five seconds is not going to finish
    // because we keep waiting.
    result.delivering = !deliveries->wait_for_others_before(cutoff, delivery_wait_);
    previous.reset();  // see clear()
    return result;
  }

  // Deliver to the subscriber list as it was when the line arrived, with
  // mutex_ released: a subscriber is module code (handleMessage), and it may
  // unload modules - itself included - from inside its handler. The copy
  // keeps every subscriber alive for this delivery even if it is removed
  // meanwhile; remove() then waits for the delivery before returning. A
  // line a handler logged is not delivered at all - see do_log.
  void on_log_message(const std::string &data) override {
    if (!has_subscribers_.load(std::memory_order_acquire)) return;
    if (is_handler_line(data)) return;
    // Every tracker is entered under mutex_, before the first call, so a
    // remove() that takes its cutoff afterwards waits for this delivery
    // whichever subscriber it is removing. The guards are declared before
    // the snapshot so that the snapshot - and with it the last reference a
    // removed subscriber may have - is released while this thread still
    // counts as delivering and before a tracker wakes a waiting remove(). A
    // plugin destroyed by that release may log from its teardown; that line
    // is then a handler line too.
    std::vector<threads::in_flight::guard> delivering;
    const depth_guard nested;
    subscribers_ptr snapshot;
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      if (!subscribers_ || subscribers_->empty()) return;
      snapshot = subscribers_;
      delivering.reserve(snapshot->size());
      for (const entry &e : *snapshot) {
        delivering.emplace_back(*e.deliveries);
        delivering.back().enter();
      }
    }
    for (std::size_t i = 0; i < snapshot->size(); ++i) {
      (*snapshot)[i].subscriber->on_log_message(data);
      delivering[i].leave();
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
  std::vector<logging_subscriber_instance> clear_subscribers() override;
  bool startup() override;
  bool shutdown() override;
  void configure() override;

  // The origin tag: a line a handler logged from inside a delivery carries
  // from_log_handler in its LogEntry, set by do_log on the logging thread,
  // where the depth is known, and read back here however the line travelled
  // in between. Keyed on the line itself rather than on anything the logger
  // remembers, so a byte-identical line from another source can neither
  // steal the tag nor inherit it.
  static std::string tag_handler_line(const std::string &data);
  static bool is_handler_line(const std::string &data);
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
