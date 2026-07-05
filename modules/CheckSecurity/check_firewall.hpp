// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace firewall_filter {

// One Windows firewall profile (Domain / Private / Public).
struct filter_obj {
  filter_obj() : enabled(0), active(0) {}

  std::string get_profile() const { return profile; }
  long long get_enabled() const { return enabled; }
  long long get_active() const { return active; }
  std::string get_inbound() const { return inbound; }
  std::string get_outbound() const { return outbound; }
  std::string show() const { return profile; }

  std::string profile;
  long long enabled;  // 1 = enabled, 0 = disabled
  long long active;   // 1 = profile is currently applied to a connected network
  std::string inbound;
  std::string outbound;
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace firewall_filter

namespace firewall_source {
// Populate the firewall profiles for the current platform. On Unix this is a
// stub that sets `error` (there is no Windows-profile firewall model on Linux).
void gather(std::vector<firewall_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace firewall_source

namespace check_firewall_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
