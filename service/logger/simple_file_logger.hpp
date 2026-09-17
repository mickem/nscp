// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nsclient/logger/log_driver_interface_impl.hpp>

#include <boost/thread/mutex.hpp>
#include <string>

namespace nsclient {
namespace logging {
namespace impl {
class simple_file_logger : public log_driver_interface_impl {
  // With `--log-backend threaded-file` (the service default) do_log runs only
  // on the logger's own worker, but with the bare `file` backend it runs on
  // every calling thread: two of them could both see the file over max_size_
  // and truncate it concurrently, and asynch_configure() rewrote these three
  // members while others were reading them. One mutex covers the state and the
  // write, which is what the threaded backend was giving the default install
  // for free.
  mutable boost::mutex mutex_;
  std::string file_;
  std::size_t max_size_;
  std::string format_;

 public:
  explicit simple_file_logger(std::string file);
  std::string base_path();

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