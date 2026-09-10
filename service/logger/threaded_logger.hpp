// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <memory>
#include <boost/thread.hpp>
#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <string>
#include <threads/concurrent_queue.hpp>

namespace nsclient {
namespace logging {
namespace impl {
class threaded_logger : public log_driver_interface_impl {
  // Shared with the worker thread: a worker that failed to exit at shutdown
  // (stuck in a subscriber, or a full pipe) is abandoned with a live queue
  // instead of having it, its mutex and its condition destroyed under it.
  struct shared_state {
    concurrent_queue<std::string> queue;
    std::atomic<bool> abandoned{false};
    // Everything thread_proc touches lives here rather than on the logger.
    // An abandoned worker outlives the threaded_logger, so it must not reach
    // back into it - nor into the nsclient_logger that owns it. A shared_ptr,
    // so the sink stays alive for as long as the worker can still write to it.
    log_driver_instance background_logger;
    // The subscriber manager is the nsclient_logger that owns this logger, so
    // it cannot be kept alive by holding a reference to it. shutdown() clears
    // it under the mutex before abandoning the worker, which also waits out an
    // on_log_message call already in flight.
    logging_subscriber *subscriber_manager = nullptr;
    boost::mutex subscriber_mutex;
    // Snapshot of the two rendering flags, taken at startup. They cannot
    // change afterwards: set_config(std::string) is overridden here to forward
    // to the background logger, so the base class flags are never written.
    bool oneline = false;
    bool no_std_err = false;
  };
  std::shared_ptr<shared_state> state_;
  boost::thread thread_;
  // How long shutdown() waits for the worker before abandoning it. Only the
  // tests change it; they cannot afford to wait out the real timeout.
  boost::posix_time::time_duration join_timeout_ = boost::posix_time::seconds(10);

 public:
  threaded_logger(logging_subscriber *subscriber_manager, log_driver_instance background_logger);
  ~threaded_logger() override;

  void do_log(std::string data) override;
  void push(const std::string &data);

  // Static, and takes nothing but the shared state: the worker can be detached
  // and outlive the threaded_logger, so the compiler must not let it touch
  // `this`.
  static void thread_proc(std::shared_ptr<shared_state> state);

  void asynch_configure() override;
  void synch_configure() override;
  bool startup() override;
  bool shutdown() override;

  void set_config(const std::string &key) override;

  void set_join_timeout_for_test(const boost::posix_time::time_duration timeout) { join_timeout_ = timeout; }
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient