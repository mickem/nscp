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

class log_driver_interface_impl : public log_driver_interface {
  bool console_log_;
  bool oneline_;
  bool no_std_err_;
  bool is_running_;

 public:
  log_driver_interface_impl() : console_log_(false), oneline_(false), no_std_err_(false), is_running_(false) {}
  ~log_driver_interface_impl() override = default;

  bool is_console() const override { return console_log_; }
  bool is_oneline() const override { return oneline_; }
  bool is_no_std_err() const override { return no_std_err_; }
  bool shutdown() override {
    is_running_ = false;
    return true;
  }
  bool startup() override {
    is_running_ = true;
    return true;
  }
  bool is_started() const override { return is_running_; }

  void set_config(const log_driver_instance other) override {
    if (other->is_console()) set_config("console");
    if (other->is_no_std_err()) set_config("no-std-err");
    if (other->is_oneline()) set_config("oneline");
    if (other->is_started()) startup();
  }

  void set_config(const std::string &key) override {
    if (key == "console")
      console_log_ = true;
    else if (key == "oneline")
      oneline_ = true;
    else if (key == "no-std-err")
      no_std_err_ = true;
    else
      do_log(log_message_factory::create_error("logger", __FILE__, __LINE__, "Invalid key: " + key));
  }
};

}  // namespace logging
}  // namespace nsclient