// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <string>
#include <vector>

namespace nsclient {
namespace logging {
namespace impl {
class simple_console_logger : public log_driver_interface_impl {
  std::string format_;
  std::vector<char> buf_;
  logging_subscriber *subscriber_manager_;

 public:
  simple_console_logger(logging_subscriber *subscriber_manager);
  ~simple_console_logger() override;
  void do_log(std::string data) override;
  struct config_data {
    std::string format;
  };
  config_data do_config();
  void synch_configure() override;
  void asynch_configure() override;
};
}  // namespace impl
}  // namespace logging
}  // namespace nsclient