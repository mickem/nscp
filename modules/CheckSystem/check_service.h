// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdint>
#include <diagnostics/service_tables.hpp>
#include <string>

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
// Whether a service belongs in the services.problems diagnostics table: what
// check_service's default critical (`not state_is_ok()`) flags, plus any
// service that stopped with an error other than "never started".
bool is_problem(DWORD state, DWORD start_type, bool delayed, bool trigger, DWORD exit_code);
// The executable of a service's ImagePath, without its arguments: they can
// carry credentials, and the diagnostics tables never show a command line.
std::string binary_of(const std::string &image_path);
// One services.problems row from a service's status and configuration.
// `started` is when its current process started (0 when it has none).
service_tables::service_row to_problem_row(const win_list_services::service_info &info, std::int64_t started);
parsers::where::node_type state_is_ok(parsers::where::value_type type, const parsers::where::evaluation_context &context,
                                      const parsers::where::node_type &subject);
parsers::where::node_type state_is_perfect(parsers::where::value_type type, const parsers::where::evaluation_context &context,
                                           const parsers::where::node_type &subject);
}  // namespace check_svc_filter
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

// The services.problems table: every Win32 service is_problem() picks, read
// from the local service control manager. Only the services whose status
// already looks wrong are opened for their configuration. Throws when the
// service control manager cannot be enumerated at all.
service_tables::sample collect_problems();
}  // namespace service_checks
