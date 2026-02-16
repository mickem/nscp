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

#include <list>
#include <nscapi/dll_defines.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <string>
#include <vector>

namespace nscapi {
namespace protobuf {
namespace functions {

// Query data to nagios string conversion
NSCAPI_EXPORT std::string query_data_to_nagios_string(const PB::Commands::QueryResponseMessage &message, std::size_t max_length);
NSCAPI_EXPORT std::string query_data_to_nagios_string(const PB::Commands::QueryResponseMessage_Response &p, std::size_t max_length);

// Query request creation
NSCAPI_EXPORT void create_simple_query_request(std::string command, const std::list<std::string> &arguments, std::string &buffer);
NSCAPI_EXPORT void create_simple_query_request(std::string command, const std::vector<std::string> &arguments, std::string &buffer);

// Query request parsing
NSCAPI_EXPORT void parse_simple_query_request(std::list<std::string> &args, const std::string &request);

// Query response parsing
NSCAPI_EXPORT int parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf, std::size_t max_length);

// Query payload append functions
NSCAPI_EXPORT void append_simple_query_response_payload(PB::Commands::QueryResponseMessage_Response *payload, std::string command, nagiosReturn ret,
                                                        std::string msg, const std::string &perf = "");
NSCAPI_EXPORT void append_simple_query_request_payload(PB::Commands::QueryRequestMessage_Request *payload, std::string command,
                                                       const std::vector<std::string> &arguments);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
