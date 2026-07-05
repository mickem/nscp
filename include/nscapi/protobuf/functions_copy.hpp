// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace nscapi {
namespace protobuf {
namespace functions {

// Copy response functions - Query target
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::QueryResponseMessage_Response *target,
                                 const ::PB::Commands::ExecuteResponseMessage_Response &source);
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::QueryResponseMessage_Response *target,
                                 const ::PB::Commands::SubmitResponseMessage_Response &source);
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::QueryResponseMessage_Response *target,
                                 const ::PB::Commands::QueryResponseMessage_Response &source);

// Copy response functions - Execute target
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::ExecuteResponseMessage_Response *target,
                                 const ::PB::Commands::ExecuteResponseMessage_Response &source);
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::ExecuteResponseMessage_Response *target,
                                 const ::PB::Commands::SubmitResponseMessage_Response &source);
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::ExecuteResponseMessage_Response *target,
                                 const ::PB::Commands::QueryResponseMessage_Response &source);

// Copy response functions - Submit target
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::SubmitResponseMessage_Response *target,
                                 const ::PB::Commands::ExecuteResponseMessage_Response &source);
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::SubmitResponseMessage_Response *target,
                                 const ::PB::Commands::SubmitResponseMessage_Response &source);
NSCAPI_EXPORT void copy_response(const std::string &command, ::PB::Commands::SubmitResponseMessage_Response *target,
                                 const ::PB::Commands::QueryResponseMessage_Response &source);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
