// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>

namespace nsclient {
namespace logging {

struct log_driver_interface {
  log_driver_interface() = default;
  virtual ~log_driver_interface() = default;
  virtual void do_log(std::string data) = 0;
  virtual void synch_configure() = 0;
  virtual void asynch_configure() = 0;

  virtual void set_config(const std::string &key) = 0;
  virtual void set_config(std::shared_ptr<log_driver_interface> other) = 0;
  virtual bool shutdown() = 0;
  virtual bool startup() = 0;
  virtual bool is_console() const = 0;
  virtual bool is_oneline() const = 0;
  virtual bool is_no_std_err() const = 0;
  virtual bool is_started() const = 0;
};

typedef std::shared_ptr<log_driver_interface> log_driver_instance;

}  // namespace logging
}  // namespace nsclient