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
  // One subscriber behind its own gate (threads::gated). A delivery enters
  // a subscriber's gate only around the call into that subscriber, so
  // closing one waits for the lines inside *it* and for nothing else: a line
  // stuck in ElasticClient neither refuses the unload of WEBServer nor holds
  // a reference to it. And the gate, not the delivery, owns the subscriber,
  // so whoever drops it is its last holder and a module's destructor never
  // runs on the logging thread.
  //
  // A closed subscriber keeps its place in the list until it is dropped, the
  // shape the plugin manager's walk lists have: a refused unload reopens it
  // where it was, and a line still inside it keeps it closed there, so a
  // retried unload, a purge or shutdown waits for that line again rather
  // than finding an empty gate and tearing the module down under it.
  typedef threads::gated<logging_subscriber_instance> gate_type;
  struct entry {
    // Names the subscriber in a handler line's chain (see do_log).
    std::uint64_t id;
    std::shared_ptr<gate_type> gate;
  };
  typedef std::vector<entry> subscribers_type;
  typedef std::shared_ptr<const subscribers_type> subscribers_ptr;

  log_driver_instance backend_;
  // Copy-on-write: the mutators publish a new vector, and a delivery walks
  // the one it started with. The list changes on module load and unload
  // only, so a log line costs one shared_ptr copy.
  subscribers_ptr subscribers_;
  std::uint64_t next_id_ = 1;
  // Mirrors "subscribers_ is non-empty", maintained under mutex_, so the
  // common case - no log-handler module loaded, as in every CLI mode - costs
  // a log line no lock at all.
  std::atomic<bool> has_subscribers_{false};
  // Guards subscribers_ and next_id_. Held for those few lines only, never
  // across a subscriber's on_log_message or a wait, so a plain blocking
  // mutex is safe. It used to be a 5 s timed mutex held across the whole
  // fan-out: a subscriber that logged from inside its handler re-entered on
  // the same thread, waited the 5 s and lost the line, a removal arriving
  // during a slow delivery gave up after 5 s and left the plugin's
  // shared_ptr in the list - the static-destruction hazard
  // plugin_manager.hpp documents - and one stuck handler cost every other
  // thread 5 s per line. Handlers run concurrently now, on the console
  // backend; the threaded backend still delivers from its one worker.
  mutable boost::mutex mutex_;
  // How long closing a subscriber waits for the lines inside it: 5 s in the
  // service, like dll_plugin's wait for its dispatchers. Settable so a test
  // can see the timed-out path in less.
  std::chrono::milliseconds delivery_wait_{5000};

  // The delivery the calling thread is inside, for do_log: the chain of
  // handlers the line being delivered already came through, and the one
  // being called now. Thread-local, so do_log reads it without a lock; the
  // process has one nsclient_logger, so one slot per thread is enough.
  struct delivery_context {
    const log_handler_chain *chain;
    std::uint64_t handler;
  };
  static delivery_context *&current_delivery() {
    static thread_local delivery_context *context = nullptr;
    return context;
  }

 public:
  nsclient_logger();
  ~nsclient_logger() override;

  // For tests: how long the bounded wait runs (see delivery_wait_).
  void set_delivery_wait(std::chrono::milliseconds wait) { delivery_wait_ = wait; }
  // Install a backend: set_backend(name) builds one and hands it here, and
  // a test hands its own.
  void use_backend(log_driver_instance backend);

  // A line from the backend: delivered to every subscriber. A line a
  // handler wrote comes back through on_handler_log_message instead, with
  // the chain of handlers it came through, and skips those. Each call goes
  // through the subscriber's gate; one closed since the list was read is
  // skipped.
  void on_log_message(const std::string &data) override;
  void on_handler_log_message(const std::string &data, const log_handler_chain &chain) override;

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

  // A line logged from inside a handler names that handler and the chain of
  // the line it was handed, and the backend carries the chain alongside the
  // line - never in it - back to on_handler_log_message, which hands it to
  // none of them. It still reaches the console, the file and every other
  // handler: a failure ElasticClient logs from its handler shows in the web
  // UI's live log and in check_nscp's error tally. A handler that logged
  // once per line would otherwise feed itself forever, and two that did
  // would feed each other; the chain grows by one handler per hop, so no
  // line goes round more than once per subscriber. A module that hands
  // lines to a thread of its own and logs from there is outside any
  // delivery and carries no chain: it has to filter by sender, as
  // ElasticClient and DotnetPlugins do.
  void do_log(std::string data) override;

  void set_backend(std::string backend) override;
  void destroy() override;

  void add_subscriber(logging_subscriber_instance) override;
  unsubscribe_result close_subscriber(logging_subscriber_instance subscriber) override;
  void reopen_subscriber(logging_subscriber_instance subscriber) override;
  bool drop_subscriber(logging_subscriber_instance subscriber) override;
  unsubscribe_result remove_subscriber(logging_subscriber_instance subscriber) override;
  std::vector<logging_subscriber_instance> clear_subscribers() override;
  bool startup() override;
  bool shutdown() override;
  void configure() override;

 private:
  void deliver(const std::string &data, const log_handler_chain &chain);
  // Under mutex_: the entry for `subscriber`, or null.
  const entry *find_locked(const logging_subscriber_instance &subscriber) const;
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
