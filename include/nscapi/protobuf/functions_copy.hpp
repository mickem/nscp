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
