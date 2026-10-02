// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/logger/log_driver_interface_impl.hpp>
#include <cstdint>
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

  // Moves the last `keep` bytes of a file that was `size` bytes long to its
  // front and cuts it to `keep`. Only resizes once the whole tail was copied:
  // when the file shrank since `size` was taken (another process sharing the
  // log truncated it first) or a write fails, it throws and leaves the length
  // alone rather than cutting the log down to whatever was copied.
  static void truncate_to_tail(const std::string &file, std::uintmax_t size, std::uintmax_t keep);

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