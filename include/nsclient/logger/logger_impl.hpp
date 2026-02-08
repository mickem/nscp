/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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