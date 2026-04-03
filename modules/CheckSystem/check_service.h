/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include "nscapi/protobuf/command.hpp"
#include "parsers/filter/modern_filter.hpp"
#include "parsers/where/filter_handler_impl.hpp"
#include "win/services.hpp"

namespace service_checks {
namespace check_svc_filter {
typedef win_list_services::service_info filter_obj;
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Exposed for unit testing
bool check_state_is_perfect(DWORD state, DWORD start_type, bool trigger);
bool check_state_is_ok(DWORD state, DWORD start_type, bool delayed, bool trigger, DWORD exit_code);
}  // namespace check_svc_filter
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace service_checks
