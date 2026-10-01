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

// Make every free-text field (result messages, perfdata aliases) well-formed
// UTF-8 before the message is serialized. protobuf refuses a string field that
// is not, and the core then cannot parse the response at all, so one stray
// ANSI byte from an OS error message would cost the caller the whole result.
// Text that is already UTF-8 is left untouched (see utf8::make_valid).
NSCAPI_EXPORT void make_valid_utf8(::PB::Commands::QueryResponseMessage &message);
NSCAPI_EXPORT void make_valid_utf8(::PB::Commands::ExecuteResponseMessage &message);
NSCAPI_EXPORT void make_valid_utf8(::PB::Commands::SubmitResponseMessage &message);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
