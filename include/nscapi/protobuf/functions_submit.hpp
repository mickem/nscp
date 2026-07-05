// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
