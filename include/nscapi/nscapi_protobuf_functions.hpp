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
/*
#include <NSCAPI.h>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_protobuf.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>
*/

#include <nscapi/dll_defines.hpp>

#include <string>
#include <list>
#include <vector>

namespace Plugin {
	class QueryResponseMessage_Response_Line;
	class Common_PerformanceData;
	class ExecuteRequestMessage_Request;
	class QueryRequestMessage_Request;
	class Common_Header;
	class ExecuteResponseMessage_Response;
	class QueryResponseMessage_Response;
	class QueryResponseMessage;
	class SubmitResponseMessage_Response;
}
namespace nscapi {
	namespace protobuf {
		namespace functions {

			typedef int nagiosReturn;
			struct settings_query_data;
			struct settings_query_key_values_data;
			class NSCAPI_EXPORT settings_query {
				settings_query_data *pimpl;

			public:
				struct NSCAPI_EXPORT key_values {
					settings_query_key_values_data *pimpl;
					key_values(std::string path);
					key_values(std::string path, std::string key, std::string str_value);
					key_values(std::string path, std::string key, long long int_value);
					key_values(std::string path, std::string key, bool bool_value);
					key_values(const key_values &other);
					key_values& operator= (const key_values &other);
					~key_values();
					bool matches(const char* path) const;
					bool matches(const char* path, const char* key) const;
					bool matches(const std::string &path) const;
					bool matches(const std::string &path, const std::string &key) const;
					std::string get_string() const;
					std::string path() const;
					std::string key() const;
					bool get_bool() const;
					long long get_int() const;
				};
				settings_query(int plugin_id);
				~settings_query();

				void get(const std::string path, const std::string key, const std::string def);
				void get(const std::string path, const std::string key, const char* def);
				void get(const std::string path, const std::string key, const long long def);
				void get(const std::string path, const std::string key, const bool def);
				void list(const std::string path, const bool recursive = false);

				void set(const std::string path, const std::string key, std::string value);
				void erase(const std::string path, const std::string key);
				const std::string request() const;
				std::string& response() const;
				bool validate_response() const;
				std::list<key_values> get_query_key_response() const;
				std::string get_response_error() const;
				void save();
				void load();
				void reload();
			};

			NSCAPI_EXPORT std::string query_data_to_nagios_string(const Plugin::QueryResponseMessage &message, std::size_t max_length);
			NSCAPI_EXPORT std::string query_data_to_nagios_string(const Plugin::QueryResponseMessage_Response &p, std::size_t max_length);

			NSCAPI_EXPORT void set_response_good(::Plugin::QueryResponseMessage_Response &response, std::string message);
			NSCAPI_EXPORT void set_response_good(::Plugin::ExecuteResponseMessage_Response &response, std::string message);
			NSCAPI_EXPORT void set_response_good(::Plugin::SubmitResponseMessage_Response &response, std::string message);
			NSCAPI_EXPORT void set_response_good_wdata(::Plugin::QueryResponseMessage_Response &response, std::string message);
			NSCAPI_EXPORT void set_response_good_wdata(::Plugin::ExecuteResponseMessage_Response &response, std::string message);

			NSCAPI_EXPORT void set_response_good_wdata(::Plugin::SubmitResponseMessage_Response &response, std::string message);
			NSCAPI_EXPORT void set_response_bad(::Plugin::QueryResponseMessage_Response &response, std::string message);
			NSCAPI_EXPORT void set_response_bad(::Plugin::ExecuteResponseMessage_Response &response, std::string message);
			NSCAPI_EXPORT void set_response_bad(::Plugin::SubmitResponseMessage_Response &response, std::string message);

			// 			NSCAPI_EXPORT Plugin::Common_ResultCode parse_nagios(const std::string &status);
			// 			NSCAPI_EXPORT Plugin::Common_ResultCode nagios_status_to_gpb(int ret);
			// 			NSCAPI_EXPORT int gbp_to_nagios_status(Plugin::Common::ResultCode ret);

			NSCAPI_EXPORT void make_submit_from_query(std::string &message, const std::string channel, const std::string alias = "", const std::string target = "", const std::string source = "");
			NSCAPI_EXPORT void make_query_from_exec(std::string &data);
			NSCAPI_EXPORT void make_query_from_submit(std::string &data);
			NSCAPI_EXPORT void make_exec_from_submit(std::string &data);
			NSCAPI_EXPORT void make_return_header(::Plugin::Common_Header *target, const ::Plugin::Common_Header &source);

			NSCAPI_EXPORT void create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer);
			NSCAPI_EXPORT void create_simple_query_request(std::string command, std::vector<std::string> arguments, std::string &buffer);
			NSCAPI_EXPORT void create_simple_submit_request(std::string channel, std::string command, nagiosReturn ret, std::string msg, std::string perf, std::string &buffer);
			NSCAPI_EXPORT void create_simple_submit_response_ok(const std::string channel, const std::string command, const std::string msg, std::string &buffer);

			NSCAPI_EXPORT bool parse_simple_submit_response(const std::string &request, std::string &response);

			NSCAPI_EXPORT void append_simple_query_response_payload(Plugin::QueryResponseMessage_Response *payload, std::string command, nagiosReturn ret, std::string msg, std::string perf = "");
			NSCAPI_EXPORT void append_simple_exec_response_payload(Plugin::ExecuteResponseMessage_Response *payload, std::string command, int ret, std::string msg);
			NSCAPI_EXPORT void append_simple_submit_response_payload(Plugin::SubmitResponseMessage_Response *payload, std::string command, bool status, std::string msg);
			NSCAPI_EXPORT void append_simple_query_request_payload(Plugin::QueryRequestMessage_Request *payload, std::string command, std::vector<std::string> arguments);
			NSCAPI_EXPORT void append_simple_exec_request_payload(Plugin::ExecuteRequestMessage_Request *payload, std::string command, std::vector<std::string> arguments);
			NSCAPI_EXPORT void parse_simple_query_request(std::list<std::string> &args, const std::string &request);
			NSCAPI_EXPORT int parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf, std::size_t max_length);
			NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::list<std::string> & args, std::string &request);
			NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::vector<std::string> & args, std::string &request);
			NSCAPI_EXPORT int parse_simple_exec_response(const std::string &response, std::list<std::string> &result);

			NSCAPI_EXPORT int create_simple_exec_response(const std::string &command, nagiosReturn ret, const std::string result, std::string &response);
			NSCAPI_EXPORT int create_simple_exec_response_unknown(std::string command, std::string result, std::string &response);

			NSCAPI_EXPORT void parse_performance_data(Plugin::QueryResponseMessage_Response_Line *payload, const std::string &perf);
			static const std::size_t no_truncation = 0;
			NSCAPI_EXPORT std::string build_performance_data(Plugin::QueryResponseMessage_Response_Line const &payload, std::size_t max_length);

			NSCAPI_EXPORT std::string extract_perf_value_as_string(const ::Plugin::Common_PerformanceData &perf);
			NSCAPI_EXPORT long long extract_perf_value_as_int(const ::Plugin::Common_PerformanceData &perf);
			NSCAPI_EXPORT std::string extract_perf_maximum_as_string(const ::Plugin::Common_PerformanceData &perf);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::QueryResponseMessage_Response* target, const ::Plugin::ExecuteResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::QueryResponseMessage_Response* target, const ::Plugin::SubmitResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::QueryResponseMessage_Response* target, const ::Plugin::QueryResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage_Response* target, const ::Plugin::ExecuteResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage_Response* target, const ::Plugin::SubmitResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage_Response* target, const ::Plugin::QueryResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::SubmitResponseMessage_Response* target, const ::Plugin::ExecuteResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::SubmitResponseMessage_Response* target, const ::Plugin::SubmitResponseMessage_Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::SubmitResponseMessage_Response* target, const ::Plugin::QueryResponseMessage_Response source);
		}
	}
}
