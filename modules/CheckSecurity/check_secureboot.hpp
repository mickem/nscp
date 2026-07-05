// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace secureboot_filter {

// A single result describing the UEFI Secure Boot state.
struct filter_obj {
  filter_obj() : enabled(0), supported(0) {}
  long long get_enabled() const { return enabled; }
  long long get_supported() const { return supported; }
  std::string show() const { return enabled ? "enabled" : "disabled"; }

  long long enabled;    // 1 if UEFI Secure Boot is enabled
  long long supported;  // 1 if the platform exposes the Secure Boot state
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace secureboot_filter

namespace secureboot_source {
// Windows only (registry SecureBoot\State); the Unix stub sets `error`.
void gather(std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace secureboot_source

namespace check_secureboot_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
