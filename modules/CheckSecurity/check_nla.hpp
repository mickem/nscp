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

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace nla_filter {

// One network as seen by Network Location Awareness.
struct filter_obj {
  filter_obj() : connected(0) {}
  std::string get_network() const { return network; }
  std::string get_category() const { return category; }
  long long get_connected() const { return connected; }
  std::string show() const { return network; }

  std::string network;   // network name
  std::string category;  // public / private / domain
  long long connected;   // 1 if the network is currently connected
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace nla_filter

namespace nla_source {
// Windows only (COM INetworkListManager); the Unix stub sets `error`.
void gather(std::vector<nla_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace nla_source

namespace check_nla_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
