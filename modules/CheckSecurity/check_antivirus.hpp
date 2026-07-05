// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace antivirus_filter {

// One registered antivirus product (Windows Security Center).
struct filter_obj {
  filter_obj() : enabled(0), up_to_date(0), product_state(0) {}
  std::string get_name() const { return name; }
  long long get_enabled() const { return enabled; }
  long long get_up_to_date() const { return up_to_date; }
  long long get_product_state() const { return product_state; }
  std::string show() const { return name; }

  std::string name;         // product display name
  long long enabled;        // 1 if real-time protection is on
  long long up_to_date;     // 1 if definitions are current
  long long product_state;  // raw productState bitfield
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace antivirus_filter

namespace antivirus_source {
// Windows only (WMI root\SecurityCenter2); the Unix stub sets `error`.
void gather(std::vector<antivirus_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace antivirus_source

namespace check_antivirus_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
