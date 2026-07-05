// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include "nscapi/protobuf/command.hpp"
#include "parsers/filter/modern_filter.hpp"
#include "parsers/where/filter_handler_impl.hpp"
#include "win/services.hpp"

namespace service_checks {
namespace check_svc_filter {
typedef win_list_services::service_info filter_obj;
typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Exposed for unit testing
bool check_state_is_perfect(DWORD state, DWORD start_type, bool trigger);
bool check_state_is_ok(DWORD state, DWORD start_type, bool delayed, bool trigger, DWORD exit_code);
}  // namespace check_svc_filter
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace service_checks
