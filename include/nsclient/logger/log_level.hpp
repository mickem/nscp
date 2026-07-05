// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace nsclient {
namespace logging {

class log_level {
  static constexpr int off = 0;       // Used to disable logging
  static constexpr int critical = 1;  // Critical error
  static constexpr int error = 2;     // Error
  static constexpr int warning = 3;   // Warning
  static constexpr int info = 10;     // information
  static constexpr int debug = 50;    // Debug messages
  static constexpr int trace = 99;    // Trace messages

  int current_level_;

 public:
  log_level() : current_level_(info) {}

  bool should_trace() const { return current_level_ >= trace; }
  bool should_debug() const { return current_level_ >= debug; }
  bool should_info() const { return current_level_ >= info; }
  bool should_warning() const { return current_level_ >= warning; }
  bool should_error() const { return current_level_ >= error; }
  bool should_critical() const { return current_level_ >= critical; }

  bool set(const std::string& level);

  std::string get() const;
};

}  // namespace logging
}  // namespace nsclient