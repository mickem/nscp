// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <string>

namespace nsclient {
namespace logging {
namespace impl {
class simple_file_logger : public log_driver_interface_impl {
  std::string file_;
  std::size_t max_size_;
  std::string format_;

 public:
  explicit simple_file_logger(std::string file);
  std::string base_path();

  // The resolved target, after configuration. Empty means file logging is
  // switched off (`file name = none`). Exposed so a test can assert the
  // sentinel was honoured rather than inferring it from which files happen to
  // exist - the mangled-sentinel bug wrote to a real path outside the test's
  // temp directory, which no existence check in that directory could see.
  const std::string &get_file() const { return file_; }

  void do_log(std::string data) override;
  struct config_data {
    std::string file;
    std::string format;
    std::size_t max_size;
  };
  config_data do_config(bool log_fault);
  void synch_configure() override;
  void asynch_configure() override;
  bool shutdown() override { return true; }
};

}  // namespace impl
}  // namespace logging
}  // namespace nsclient