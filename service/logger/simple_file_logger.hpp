// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdint>
#include <string>

namespace nsclient {
namespace logging {
namespace impl {
// The log file: appends one formatted line per entry, creates the file 0640
// and cuts it back to its newest 70% past `max size`. Not thread safe on its
// own; nsclient_logger serialises every call.
class simple_file_logger {
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

  void do_log(const std::string &data);
  // Defaults are what apply() gets when the settings cannot be read at all:
  // keep the file, date the lines, and never truncate.
  struct config_data {
    std::string file;
    std::string format = "%Y-%m-%d %H:%M:%S";
    std::size_t max_size = 0;
  };
  // Registers the keys and reads them; touches nothing on this object, so
  // it can run without the caller's lock (reading settings may log).
  static config_data do_config(bool log_fault);
  void apply(const config_data &config);
  void synch_configure();
  void asynch_configure();
};

}  // namespace impl
}  // namespace logging
}  // namespace nsclient