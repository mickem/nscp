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
  // removing one waits for the lines inside *it* and for nothing else: a
  // line stuck in ElasticClient neither refuses the unload of WEBServer nor
  // holds a reference to it. And the gate, not the delivery, owns the
  // subscriber, so the remover is its last holder and a module's destructor
  // never runs on the logging thread.
  typedef threads::gated<logging_subscriber_instance> gate_type;
  struct entry {
    // Names the subscriber in a handler line's chain (see do_log).
    std::uint64_t id;
    std::shared_ptr<gate_type> gate;
  };
  typedef std::vector<entry> subscribers_type;
  typedef std::shared_ptr<const subscribers_type> subscribers_ptr;
  // A subscriber taken off the list whose wait ran out with a line still
  // inside it. It stays closed, and clear() waits for it again at shutdown
  // and reports it if it is still stuck.
  struct draining {
    entry subscriber;
    std::uint64_t cutoff;
  };

  log_driver_instance backend_;
  // Copy-on-write: add() / remove() / clear() publish a new vector, and a
  // delivery walks the one it started with. The list changes on module
  // load and unload only, so a log line costs one shared_ptr copy.
  subscribers_ptr subscribers_;
  std::vector<draining> draining_;
  std::uint64_t next_id_ = 1;
  // Mirrors "subscribers_ is non-empty", maintained under mutex_, so the
  // common case - no log-handler module loaded, as in every CLI mode - costs
  // a log line no lock at all.
  std::atomic<bool> has_subscribers_{false};
  // Guards subscribers_, draining_ and next_id_. Held for those few lines
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

  // The delivery the calling thread is inside, for do_log: the chain of
  // handlers the line being delivered already came through, and the one
  // being called now. Thread-local, so do_log reads it without a lock; the
  // process has one nsclient_logger, so one slot per thread is enough.
  struct delivery_context {
    const std::vector<std::uint64_t> *chain;
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

  // Deliver the line to every subscriber that did not produce it. Each call
  // goes through the subscriber's gate (see entry); a subscriber closed for
  // removal since the list was read is skipped.
  void on_log_message(const std::string &data) override;

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

  // A line a handler logs from inside its handler carries the chain of
  // handlers it came through - the one that logged it, and the chain of the
  // line that handler was handed - in LogEntry.handled_by, and is not handed
  // to any of them again. It still reaches every other handler: a failure
  // ElasticClient logs from its handler shows in the web UI's live log and
  // in check_nscp's error tally. A handler that logged once per line it was
  // handed would otherwise feed itself forever; two that did would feed each
  // other. The chain grows by one handler per hop, so no line goes round
  // more than once per subscriber.
  //
  // The field is written ahead of the rest of the message, so a line is
  // known to carry one from its first byte, and only such a line is parsed
  // here. A module that hands lines to a thread of its own and logs from
  // there is outside any delivery and is not tagged: it has to filter by
  // sender, as ElasticClient and DotnetPlugins do.
  static std::string tag_handler_line(const std::string &data, const std::vector<std::uint64_t> &chain);
  // The chain a line carries; empty for a line no handler wrote.
  static std::vector<std::uint64_t> handler_chain(const std::string &data);

 private:
  // Under mutex_: release the draining subscribers whose lines have left
  // since, into `released` for the caller to drop after unlocking.
  void prune_draining(std::vector<logging_subscriber_instance> &released);
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient
