// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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

 public:
  unix_service() {}
  virtual ~unix_service() {}
  inline void print_debug(const std::string s) { std::cout << s << std::endl; }
  inline void print_debug(const char *s) { std::cout << s << std::endl; }

  static void handleSigTerm(int) { TBase::get_global_instance()->stop_service(); }
  static void handleSigInt(int) { TBase::get_global_instance()->stop_service(); }
  /** start */
  void start_and_wait(std::string name) {
    is_running_ = true;

    if (signal(SIGTERM, unix_service<TBase>::handleSigTerm) == SIG_ERR) handle_error(__LINE__, __FILE__, "Failed to hook SIGTERM!");
    if (signal(SIGINT, unix_service<TBase>::handleSigInt) == SIG_ERR) handle_error(__LINE__, __FILE__, "Failed to hook SIGTERM!");

    TBase::handle_startup("TODO");

    print_debug("Service started waiting for termination event...");
    {
      boost::unique_lock<boost::mutex> lock(stop_mutex_);
      while (is_running_) shutdown_condition_.wait(lock);
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
}  // namespace service_helper_impl
