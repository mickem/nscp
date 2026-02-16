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
