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