// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/logger/log_driver_interface.hpp>
#include <nsclient/logger/log_level.hpp>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger.hpp>
#include <string>

namespace nsclient {
namespace logging {

class logger_impl : public logger {
  log_level level_;

 public:
  logger_impl() = default;
  ~logger_impl() override = default;

  bool should_trace() const override { return level_.should_trace(); }
  bool should_debug() const override { return level_.should_debug(); }
  bool should_info() const override { return level_.should_info(); }
  bool should_warning() const override { return level_.should_warning(); }
  bool should_error() const override { return level_.should_error(); }
  bool should_critical() const override { return level_.should_critical(); }

  void set_log_level(const std::string level) override {
    if (!level_.set(level)) {
      do_log(log_message_factory::create_error("logger", __FILE__, __LINE__, "Invalid log level: " + level));
    }
  }
  std::string get_log_level() const override { return level_.get(); }
  void debug(const std::string &module, const char *file, const int line, const std::string &message) override {
    if (should_debug()) do_log(log_message_factory::create_debug(module, file, line, message));
  }
  void trace(const std::string &module, const char *file, const int line, const std::string &message) override {
    if (should_trace()) do_log(log_message_factory::create_trace(module, file, line, message));
  }
  void info(const std::string &module, const char *file, const int line, const std::string &message) override {
    if (should_info()) do_log(log_message_factory::create_info(module, file, line, message));
  }
  void warning(const std::string &module, const char *file, const int line, const std::string &message) override {
    if (should_warning()) do_log(log_message_factory::create_warning(module, file, line, message));
  }
  void error(const std::string &module, const char *file, const int line, const std::string &message) override {
    if (should_error()) do_log(log_message_factory::create_error(module, file, line, message));
  }
  void critical(const std::string &module, const char *file, const int line, const std::string &message) override {
    if (should_critical()) do_log(log_message_factory::create_critical(module, file, line, message));
  }
  void raw(const std::string &message) override { do_log(message); }

  virtual void do_log(std::string data) = 0;
};
}  // namespace logging
}  // namespace nsclient