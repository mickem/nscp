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

// Execute request creation
NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::list<std::string> &args, std::string &request);
NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::vector<std::string> &args,
                                              std::string &request);

// Execute response creation
NSCAPI_EXPORT int create_simple_exec_response(const std::string &command, nagiosReturn ret, const std::string &result, std::string &response);
NSCAPI_EXPORT int create_simple_exec_response_unknown(std::string command, std::string result, std::string &response);

// Execute response parsing
NSCAPI_EXPORT int parse_simple_exec_response(const std::string &response, std::list<std::string> &result);

// Execute payload append functions
NSCAPI_EXPORT void append_simple_exec_response_payload(PB::Commands::ExecuteResponseMessage_Response *payload, std::string command, int ret, std::string msg);
NSCAPI_EXPORT void append_simple_exec_request_payload(PB::Commands::ExecuteRequestMessage_Request *payload, std::string command,
                                                      const std::vector<std::string> &arguments);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
