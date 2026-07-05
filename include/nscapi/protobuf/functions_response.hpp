// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace nscapi {
namespace protobuf {
namespace functions {

// Response helper functions - set_response_good
NSCAPI_EXPORT void set_response_good(::PB::Commands::QueryResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good(::PB::Commands::SubmitResponseMessage_Response &response, std::string message);

// Response helper functions - set_response_good_wdata
NSCAPI_EXPORT void set_response_good_wdata(::PB::Commands::QueryResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good_wdata(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good_wdata(::PB::Commands::SubmitResponseMessage_Response &response, std::string message);

// Response helper functions - set_response_bad
NSCAPI_EXPORT void set_response_bad(::PB::Commands::QueryResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_bad(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_bad(::PB::Commands::SubmitResponseMessage_Response &response, std::string message);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
