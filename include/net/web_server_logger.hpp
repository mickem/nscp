// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Server.h>

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>
#include <utility>

namespace net {

// The WebLogger the HTTP servers (WEBServer, NCPAServer) hand to
// Mongoose::Server: the server's own messages go to the agent log through the
// module's logging macros, at the levels the module's `log` settings enable.
// `prefix` names the listener in the line ("NCPA: "), since both servers share
// one HTTP library and its wording. Header-only: it logs through the plugin
// singleton of whichever module includes it.
//
// Not a thread reporter: it rewords what it logs (the prefix), and the guard
// line of a dying thread must reach the log as written. A server hands its
// threads NSC_THREAD_REPORTER through Server::setThreadReporting().
class web_server_logger : public Mongoose::WebLogger {
  bool log_errors_;
  bool log_info_;
  bool log_debug_;
  std::string prefix_;

 public:
  web_server_logger(const bool log_errors, const bool log_info, const bool log_debug, std::string prefix = "")
      : log_errors_(log_errors), log_info_(log_info), log_debug_(log_debug), prefix_(std::move(prefix)) {}
  void log_error(const std::string &message) override {
    if (log_errors_) {
      NSC_LOG_ERROR(prefix_ + message);
    }
  }
  void log_info(const std::string &message) override {
    if (log_info_) {
      NSC_LOG_MESSAGE(prefix_ + message);
    }
  }
  void log_debug(const std::string &message) override {
    if (log_debug_) {
      NSC_DEBUG_MSG(prefix_ + message);
    }
  }
};

}  // namespace net
