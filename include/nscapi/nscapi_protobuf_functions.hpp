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

#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/dll_defines.hpp>

#include <string>
#include <list>
#include <vector>

namespace nscapi {
namespace protobuf {
namespace functions {

typedef int nagiosReturn;

NSCAPI_EXPORT std::string query_data_to_nagios_string(const PB::Commands::QueryResponseMessage &message, std::size_t max_length);
NSCAPI_EXPORT std::string query_data_to_nagios_string(const PB::Commands::QueryResponseMessage_Response &p, std::size_t max_length);
NSCAPI_EXPORT int gbp_to_nagios_status(PB::Common::ResultCode ret);

NSCAPI_EXPORT void set_response_good(::PB::Commands::QueryResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good(::PB::Commands::SubmitResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good_wdata(::PB::Commands::QueryResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_good_wdata(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message);

NSCAPI_EXPORT void set_response_good_wdata(::PB::Commands::SubmitResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_bad(::PB::Commands::QueryResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_bad(::PB::Commands::ExecuteResponseMessage_Response &response, std::string message);
NSCAPI_EXPORT void set_response_bad(::PB::Commands::SubmitResponseMessage_Response &response, std::string message);

NSCAPI_EXPORT void make_submit_from_query(std::string &message, const std::string channel, const std::string alias = "", const std::string target = "",
                                          const std::string source = "");
NSCAPI_EXPORT void make_query_from_exec(std::string &data);
NSCAPI_EXPORT void make_query_from_submit(std::string &data);
NSCAPI_EXPORT void make_exec_from_submit(std::string &data);
NSCAPI_EXPORT void make_return_header(PB::Common::Header *target, const PB::Common::Header &source);

NSCAPI_EXPORT void create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer);
NSCAPI_EXPORT void create_simple_query_request(std::string command, std::vector<std::string> arguments, std::string &buffer);
NSCAPI_EXPORT void create_simple_submit_request(std::string channel, std::string command, nagiosReturn ret, std::string msg, std::string perf,
                                                std::string &buffer);
NSCAPI_EXPORT void create_simple_submit_response_ok(const std::string channel, const std::string command, const std::string msg, std::string &buffer);

NSCAPI_EXPORT bool parse_simple_submit_response(const std::string &request, std::string &response);

NSCAPI_EXPORT void append_simple_query_response_payload(PB::Commands::QueryResponseMessage_Response *payload, std::string command, nagiosReturn ret,
                                                        std::string msg, std::string perf = "");
NSCAPI_EXPORT void append_simple_exec_response_payload(PB::Commands::ExecuteResponseMessage_Response *payload, std::string command, int ret, std::string msg);
NSCAPI_EXPORT void append_simple_submit_response_payload(PB::Commands::SubmitResponseMessage_Response *payload, std::string command, bool status,
                                                         std::string msg);
NSCAPI_EXPORT void append_simple_query_request_payload(PB::Commands::QueryRequestMessage_Request *payload, std::string command,
                                                       std::vector<std::string> arguments);
NSCAPI_EXPORT void append_simple_exec_request_payload(PB::Commands::ExecuteRequestMessage_Request *payload, std::string command,
                                                      std::vector<std::string> arguments);
NSCAPI_EXPORT void parse_simple_query_request(std::list<std::string> &args, const std::string &request);
NSCAPI_EXPORT int parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf, std::size_t max_length);
NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::list<std::string> &args, std::string &request);
NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::vector<std::string> &args,
                                              std::string &request);
NSCAPI_EXPORT int parse_simple_exec_response(const std::string &response, std::list<std::string> &result);

NSCAPI_EXPORT int create_simple_exec_response(const std::string &command, nagiosReturn ret, const std::string result, std::string &response);
NSCAPI_EXPORT int create_simple_exec_response_unknown(std::string command, std::string result, std::string &response);

NSCAPI_EXPORT void parse_performance_data(PB::Commands::QueryResponseMessage_Response_Line *payload, const std::string &perf);
static const std::size_t no_truncation = 0;
NSCAPI_EXPORT std::string build_performance_data(PB::Commands::QueryResponseMessage_Response_Line const &payload, std::size_t max_length);

NSCAPI_EXPORT std::string extract_perf_value_as_string(const PB::Common::PerformanceData &perf);
NSCAPI_EXPORT long long extract_perf_value_as_int(const PB::Common::PerformanceData &perf);
NSCAPI_EXPORT std::string extract_perf_maximum_as_string(const PB::Common::PerformanceData &perf);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::QueryResponseMessage_Response *target,
                                 const ::PB::Commands::ExecuteResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::QueryResponseMessage_Response *target,
                                 const ::PB::Commands::SubmitResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::QueryResponseMessage_Response *target,
                                 const ::PB::Commands::QueryResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::ExecuteResponseMessage_Response *target,
                                 const ::PB::Commands::ExecuteResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::ExecuteResponseMessage_Response *target,
                                 const ::PB::Commands::SubmitResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::ExecuteResponseMessage_Response *target,
                                 const ::PB::Commands::QueryResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::SubmitResponseMessage_Response *target,
                                 const ::PB::Commands::ExecuteResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::SubmitResponseMessage_Response *target,
                                 const ::PB::Commands::SubmitResponseMessage_Response source);
NSCAPI_EXPORT void copy_response(const std::string command, ::PB::Commands::SubmitResponseMessage_Response *target,
                                 const ::PB::Commands::QueryResponseMessage_Response source);
}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
