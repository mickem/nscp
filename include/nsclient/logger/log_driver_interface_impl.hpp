// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/logger/log_driver_interface.hpp>
#include <nsclient/logger/log_level.hpp>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger.hpp>

#include <atomic>
#include <string>

namespace nsclient {
namespace logging {

class log_driver_interface_impl : public log_driver_interface {
  // Written by set_config()/startup()/shutdown() on whatever thread the module
  // calling NSAPISetLogOption happens to use - CommandClient taking the
  // console over for its prompt is the case that matters - and read by
  // is_console() / is_started() on every thread that logs. Atomics: these are
  // flags, so there is nothing to lock, but a plain bool here is a data race
  // and a stale or torn read decides whether a line reaches the console.
  std::atomic<bool> console_log_;
  std::atomic<bool> oneline_;
  std::atomic<bool> no_std_err_;
  std::atomic<bool> is_running_;

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

  // "no-console" is how a module that has taken the console over for itself
  // asks the core to stop writing to it (see NSAPISetLogOption and the
  // CommandClient prompt, which renders log messages through its line editor
  // so they do not land in the middle of what the user is typing).
  void set_config(const std::string &key) override {
    if (key == "console")
      console_log_ = true;
    else if (key == "no-console")
      console_log_ = false;
    else if (key == "oneline")
      oneline_ = true;
    else if (key == "no-std-err")
      no_std_err_ = true;
    else
      do_log(log_message_factory::create_error("logger", __FILE__, __LINE__, "Invalid key: " + key));
  }

  // The keys set_config() above understands, as opposed to the severity names
  // log_level::set() understands. logger::set_log_level() takes both on the
  // same string argument and needs to tell them apart.
  static bool is_driver_option(const std::string &key) { return key == "console" || key == "no-console" || key == "oneline" || key == "no-std-err"; }
};

}  // namespace logging
}  // namespace nsclient