// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
