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

// Constant for no truncation
static const std::size_t no_truncation = 0;

// Performance data parsing and building
NSCAPI_EXPORT void parse_performance_data(PB::Commands::QueryResponseMessage_Response_Line *payload, const std::string &perf);
NSCAPI_EXPORT std::string build_performance_data(PB::Commands::QueryResponseMessage_Response_Line const &payload, std::size_t max_length);

// Performance data extraction
NSCAPI_EXPORT std::string extract_perf_value_as_string(const PB::Common::PerformanceData &perf);
NSCAPI_EXPORT long long extract_perf_value_as_int(const PB::Common::PerformanceData &perf);
NSCAPI_EXPORT std::string extract_perf_maximum_as_string(const PB::Common::PerformanceData &perf);

}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
