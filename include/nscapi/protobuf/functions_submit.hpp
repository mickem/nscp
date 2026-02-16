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

#include <nscapi/dll_defines.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <string>

namespace nscapi {
namespace protobuf {
namespace functions {

// Submit request creation
NSCAPI_EXPORT void create_simple_submit_request(std::string channel, std::string command, nagiosReturn ret, std::string msg, const std::string &perf,
                                                std::string &buffer);
NSCAPI_EXPORT void create_simple_submit_response_ok(const std::string &channel, const std::string &command, const std::string &msg, std::string &buffer);

// Submit response parsing
NSCAPI_EXPORT bool parse_simple_submit_response(const std::string &request, std::string &response);

// Submit payload append functions
NSCAPI_EXPORT void append_simple_submit_response_payload(PB::Commands::SubmitResponseMessage_Response *payload, std::string command, bool status,
                                                         std::string msg);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
