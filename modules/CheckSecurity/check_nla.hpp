// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
