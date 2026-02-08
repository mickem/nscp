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