// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <csignal>
#include <boost/thread/condition.hpp>
#pragma once

#include <signal.h>

#include <iostream>
#include <string>

namespace service_helper_impl {
/**
 * Helper class to implement a NT service
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 */
template <class TBase>
class unix_service : public TBase {
 private:
  boost::mutex stop_mutex_;
  bool is_running_;
  boost::condition shutdown_condition_;
  // Set from the signal handlers, which may run on any thread at any point,
  // including while the main thread holds stop_mutex_: taking a mutex there
  // is not async-signal-safe. The wait loop polls this flag instead.
  static volatile sig_atomic_t stop_signalled_;

 public:
  unix_service() : is_running_(false) {}
  virtual ~unix_service() {}
  inline void print_debug(const std::string s) { std::cout << s << std::endl; }
  inline void print_debug(const char *s) { std::cout << s << std::endl; }

  static void handleSigTerm(int) { stop_signalled_ = 1; }
  static void handleSigInt(int) { stop_signalled_ = 1; }
  /** start */
  void start_and_wait(std::string name) {
    is_running_ = true;

    if (signal(SIGTERM, unix_service<TBase>::handleSigTerm) == SIG_ERR) handle_error(__LINE__, __FILE__, "Failed to hook SIGTERM!");
    if (signal(SIGINT, unix_service<TBase>::handleSigInt) == SIG_ERR) handle_error(__LINE__, __FILE__, "Failed to hook SIGTERM!");

    TBase::handle_startup("TODO");

    print_debug("Service started waiting for termination event...");
    {
      boost::unique_lock<boost::mutex> lock(stop_mutex_);
      while (is_running_ && !stop_signalled_) shutdown_condition_.timed_wait(lock, boost::posix_time::milliseconds(200));
    }

    print_debug("Shutting down...");
    TBase::handle_shutdown("TODO");
    print_debug("Shutting down (down)...");
  }
  void stop_service() {
    {
      boost::lock_guard<boost::mutex> lock(stop_mutex_);
      is_running_ = false;
    }
    shutdown_condition_.notify_one();
  }
  static void handle_error(unsigned int line, const char *file, std::string message) {
    std::cerr << file << ":" << line << ": " << message << std::endl;
  }
};
template <class TBase>
volatile sig_atomic_t unix_service<TBase>::stop_signalled_ = 0;
}  // namespace service_helper_impl
