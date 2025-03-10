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

#include <nsclient/logger/base_logger_impl.hpp>

#include <string>

namespace nsclient {
namespace logging {
namespace impl {
class simple_file_logger : public nsclient::logging::log_driver_interface_impl {
  std::string file_;
  std::size_t max_size_;
  std::string format_;

 public:
  simple_file_logger(std::string file);
  std::string base_path();

  void do_log(const std::string data);
  struct config_data {
    std::string file;
    std::string format;
    std::size_t max_size;
  };
  config_data do_config(const bool log_fault);
  void synch_configure();
  void asynch_configure();
  bool shutdown() { return true; }
};

}  // namespace impl
}  // namespace logging
}  // namespace nsclient