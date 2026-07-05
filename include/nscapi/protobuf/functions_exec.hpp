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
