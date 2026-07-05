// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
