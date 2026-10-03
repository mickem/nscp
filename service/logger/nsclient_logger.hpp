// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/logger_impl.hpp>
#include <string>
#include <utility>
#include <vector>

#include "simple_file_logger.hpp"

namespace nsclient {
namespace logging {
namespace impl {

// The core's one logger. A line is written to the console and the log file
// on the calling thread, under one lock, so it is on disk before do_log
// returns and in order with whatever the caller prints next. Log-handler
// modules (subscribers) are fed from a single worker thread instead, so a
// slow or stuck handler costs the threads that log nothing.
//
// One delivery thread is what keeps the subscriber side small:
//  - unloading a module waits until the worker is not inside *that* module,
//    which is one pointer compare, not a gate per subscriber;
//  - a line logged on the worker thread - from inside a handler - goes to
//    the console and the file but is not handed to any subscriber, so a
//    handler that logs once per line it receives cannot feed itself or
//    another handler.
class nsclient_logger : public logger_impl {
  // Everything the worker touches. Shared with it, so a worker abandoned at
  // shutdown (stuck in a handler) returns to live state, never to a
  // destroyed logger.
  struct entry {
    std::uint64_t id;
    logging_subscriber_instance subscriber;
    // Closed by close_subscriber: kept in its place, but handed no lines.
    bool open;
  };
  typedef std::vector<entry> entries;
  struct delivery {
    boost::mutex mutex;
    boost::condition_variable changed;
    std::deque<std::string> queue;
    entries subscribers;
    std::uint64_t next_id = 1;
    // Lines ever queued, and lines handed to every handler (or dropped):
    // a flush waits for `done` to reach what `queued` was when it began.
    std::uint64_t queued = 0;
    std::uint64_t done = 0;
    // The subscriber the worker is calling right now, or null.
    logging_subscriber_instance current;
    bool started = false;
    bool stopping = false;
    // Lines dropped because the queue was full, since the last report.
    std::uint64_t dropped = 0;
  };

  // Guards the sink state below. Held while a line is written, never while
  // settings are read or a subscriber is called.
  boost::mutex sink_mutex_;
  bool console_ = false;
  bool oneline_ = false;
  bool no_std_err_ = false;
  std::unique_ptr<simple_file_logger> file_;

  std::shared_ptr<delivery> delivery_;
  std::shared_ptr<boost::thread> worker_;
  // Mirrors "delivery_->subscribers is non-empty", so a log line costs no
  // second lock when no log-handler module is loaded (every CLI mode).
  std::atomic<bool> has_subscribers_{false};
  // How long a removal waits for the worker to leave the subscriber: 5 s in
  // the service, like dll_plugin's wait for its dispatchers.
  std::chrono::milliseconds delivery_wait_{5000};
  std::chrono::milliseconds join_wait_{10000};
  // How many lines may wait for the handlers. A handler that is stuck, or
  // slower than the agent logs, would otherwise grow the queue without
  // bound; past this the oldest lines are dropped.
  std::size_t queue_limit_ = 10000;

  static bool &on_worker_thread();
  // Under delivery_->mutex: the subscriber's entry, or end().
  entries::iterator find(const logging_subscriber_instance &subscriber);
  // Under delivery_->mutex: wait until the worker is not inside `subscriber`;
  // false when the wait ran out.
  bool wait_until_left(boost::unique_lock<boost::mutex> &lock, const logging_subscriber_instance &subscriber);
  static void deliver(std::shared_ptr<delivery> d);
  void write_sinks(const std::string &data);
  void flush_handlers();
  // Under delivery_->mutex: wait until the lines queued so far have been
  // handed out, or `deadline` passes. Not on the worker itself.
  void wait_for_queued(boost::unique_lock<boost::mutex> &lock, const boost::system_time &deadline);
  // Under delivery_->mutex: drop what is queued and count it as done.
  void drop_queued();

 public:
  nsclient_logger();
  ~nsclient_logger() override;

  // For tests: shorten the bounded waits.
  void set_delivery_wait(std::chrono::milliseconds wait) { delivery_wait_ = wait; }
  void set_join_wait(std::chrono::milliseconds wait) { join_wait_ = wait; }
  void set_queue_limit(std::size_t limit) { queue_limit_ = limit; }

  // Takes both severity names ("debug", "trace", ...) and console options
  // ("console", "no-console", "oneline", "no-std-err") - cli_parser pushes
  // both onto the same list. "console" also waits for the lines queued for
  // the handlers, so a module handing the console back sees them first.
  void set_log_level(std::string level) override;
  static bool is_console_option(const std::string &key);

  void do_log(std::string data) override;

  // "file" and "threaded-file" (the old name, still in installed service
  // units) turn the log file on; anything else turns it off.
  void set_backend(std::string backend) override;
  void destroy() override;

  // Adding one that is already on the list reopens it in its place.
  void add_subscriber(logging_subscriber_instance) override;
  unsubscribe_result close_subscriber(logging_subscriber_instance subscriber) override;
  void reopen_subscriber(logging_subscriber_instance subscriber) override;
  bool drop_subscriber(logging_subscriber_instance subscriber) override;
  unsubscribe_result remove_subscriber(logging_subscriber_instance subscriber) override;
  std::vector<logging_subscriber_instance> clear_subscribers() override;
  bool startup() override;
  bool shutdown() override;
  void configure() override;
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
