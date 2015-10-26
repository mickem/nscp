/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once
#include <string>
#include <list>

#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_protobuf_types.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/dll_defines.hpp>

#include <strEx.h>


namespace nscapi {
	namespace protobuf {
		namespace functions {

//			typedef nscapi::protobuf::types::destination_container destination_container;
			typedef nscapi::protobuf::types::decoded_simple_command_data decoded_simple_command_data;
			
			class NSCAPI_EXPORT settings_query {
				::Plugin::SettingsRequestMessage request_message;
				::Plugin::SettingsResponseMessage response_message;
				std::string response_buffer;
				int plugin_id;

			public:
				struct NSCAPI_EXPORT key_values {
					std::string path;
					boost::optional<std::string> key;
					boost::optional<std::string> str_value;
					boost::optional<long long> int_value;
					boost::optional<bool> bool_value;
					key_values(std::string path) : path(path) {}
					key_values(std::string path, std::string key, std::string str_value) : path(path), key(key), str_value(str_value) {}
					key_values(std::string path, std::string key, long long int_value) : path(path), key(key), int_value(int_value) {}
					key_values(std::string path, std::string key, bool bool_value) : path(path), key(key), bool_value(bool_value) {}
					std::string get_string() const;
					bool get_bool() const;
					long long get_int() const;
				};
				settings_query(int plugin_id);

				void get(const std::string path, const std::string key, const std::string def);
				void get(const std::string path, const std::string key, const char* def);
				void get(const std::string path, const std::string key, const long long def);
				void get(const std::string path, const std::string key, const bool def);

				void set(const std::string path, const std::string key, std::string value);
				const std::string request() const;
				std::string& response() { return response_buffer; }
				bool validate_response();
				std::list<key_values> get_query_key_response() const ;
				std::string get_response_error() const;
				void save();
				void load();
				void reload();

			};

			NSCAPI_EXPORT std::string query_data_to_nagios_string(const Plugin::QueryResponseMessage &message);
			NSCAPI_EXPORT std::string query_data_to_nagios_string(const Plugin::QueryResponseMessage::Response &p);

			inline void set_response_good(::Plugin::QueryResponseMessage::Response &response, std::string message) {
				response.set_result(::Plugin::Common_ResultCode_OK);
				response.add_lines()->set_message(message);
			}
			inline void set_response_good(::Plugin::ExecuteResponseMessage::Response &response, std::string message) {
				response.set_result(::Plugin::Common_ResultCode_OK);
				response.set_message(message);
				if (!response.has_command())
					response.set_command("unknown");
			}
			inline void set_response_good(::Plugin::SubmitResponseMessage::Response &response, std::string message) {
				response.mutable_result()->set_code(::Plugin::Common_Result_StatusCodeType_STATUS_OK);
				response.mutable_result()->set_message(message);
				if (!response.has_command())
					response.set_command("unknown");
			}
			inline void set_response_good_wdata(::Plugin::QueryResponseMessage::Response &response, std::string message) {
				response.set_result(::Plugin::Common_ResultCode_OK);
				response.set_data(message);
				response.add_lines()->set_message("see data segment");
			}
			inline void set_response_good_wdata(::Plugin::ExecuteResponseMessage::Response &response, std::string message) {
				response.set_result(::Plugin::Common_ResultCode_OK);
				response.set_data(message);
				response.set_message("see data segment");
				if (!response.has_command())
					response.set_command("unknown");
			}
			
			inline void set_response_good_wdata(::Plugin::SubmitResponseMessage::Response &response, std::string message) {
				response.mutable_result()->set_code(::Plugin::Common_Result_StatusCodeType_STATUS_OK);
				response.mutable_result()->set_data(message);
				response.mutable_result()->set_message("see data segment");
			}
			inline void set_response_bad(::Plugin::QueryResponseMessage::Response &response, std::string message) {
				response.set_result(Plugin::Common_ResultCode_UNKNOWN);
				response.add_lines()->set_message(message);
				if (!response.has_command())
					response.set_command("unknown");
			}
			inline void set_response_bad(::Plugin::ExecuteResponseMessage::Response &response, std::string message) {
				response.set_result(Plugin::Common_ResultCode_UNKNOWN);
				response.set_message(message);
				if (!response.has_command())
					response.set_command("unknown");
			}
			inline void set_response_bad(::Plugin::SubmitResponseMessage::Response &response, std::string message) {
				response.mutable_result()->set_code(::Plugin::Common_Result_StatusCodeType_STATUS_ERROR);
				response.mutable_result()->set_message(message);
				if (!response.has_command())
					response.set_command("unknown");
			}

			NSCAPI_EXPORT Plugin::Common::ResultCode parse_nagios(const std::string &status);
			NSCAPI_EXPORT Plugin::Common::ResultCode nagios_status_to_gpb(int ret);
			NSCAPI_EXPORT int gbp_to_nagios_status(Plugin::Common::ResultCode ret);
			inline Plugin::Common::ResultCode gbp_status_to_gbp_nagios(Plugin::Common::Result::StatusCodeType ret) {
				if (ret == Plugin::Common_Result_StatusCodeType_STATUS_OK)
					return Plugin::Common_ResultCode_OK;
				return Plugin::Common_ResultCode_UNKNOWN;
			}
			inline Plugin::Common::Result::StatusCodeType gbp_to_nagios_gbp_status(Plugin::Common::ResultCode ret) {
				if (ret == Plugin::Common_ResultCode_UNKNOWN||ret == Plugin::Common_ResultCode_WARNING||ret == Plugin::Common_ResultCode_CRITICAL)
					return Plugin::Common_Result_StatusCodeType_STATUS_ERROR;
				return Plugin::Common_Result_StatusCodeType_STATUS_OK;
			}
			
			inline Plugin::LogEntry::Entry::Level log_to_gpb(NSCAPI::messageTypes ret) {
				if (ret == NSCAPI::log_level::critical)
					return Plugin::LogEntry_Entry_Level_LOG_CRITICAL;
				if (ret == NSCAPI::log_level::debug)
					return Plugin::LogEntry_Entry_Level_LOG_DEBUG;
				if (ret == NSCAPI::log_level::trace)
					return Plugin::LogEntry_Entry_Level_LOG_TRACE;
				if (ret == NSCAPI::log_level::error)
					return Plugin::LogEntry_Entry_Level_LOG_ERROR;
				if (ret == NSCAPI::log_level::info)
					return Plugin::LogEntry_Entry_Level_LOG_INFO;
				if (ret == NSCAPI::log_level::warning)
					return Plugin::LogEntry_Entry_Level_LOG_WARNING;
				return Plugin::LogEntry_Entry_Level_LOG_ERROR;
			}
			NSCAPI::messageTypes gpb_to_log(Plugin::LogEntry::Entry::Level ret);


			NSCAPI_EXPORT void create_simple_header(Plugin::Common::Header* hdr);
//			NSCAPI_EXPORT void add_host(Plugin::Common::Header* hdr, const destination_container &dst);
//			NSCAPI_EXPORT bool parse_destination(const ::Plugin::Common_Header &header, const std::string tag, destination_container &data, const bool expand_meta = false);

			NSCAPI_EXPORT void make_submit_from_query(std::string &message, const std::string channel, const std::string alias = "", const std::string target = "", const std::string source = "");
			NSCAPI_EXPORT void make_query_from_exec(std::string &data);
			NSCAPI_EXPORT void make_query_from_submit(std::string &data);
			NSCAPI_EXPORT void make_exec_from_submit(std::string &data);
			NSCAPI_EXPORT void make_exec_from_query(std::string &data);
			NSCAPI_EXPORT void make_return_header(::Plugin::Common_Header *target, const ::Plugin::Common_Header &source);

			NSCAPI_EXPORT void create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer);
			NSCAPI_EXPORT void create_simple_query_request(std::string command, std::vector<std::string> arguments, std::string &buffer);
			NSCAPI_EXPORT void create_simple_submit_request(std::string channel, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer);
			NSCAPI_EXPORT void create_simple_submit_response(const std::string channel, const std::string command, const ::Plugin::Common_Result_StatusCodeType result, const std::string msg, std::string &buffer);

			//NSCAPI_EXPORT int parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::string &alias, std::string &message, std::string &perf);
			NSCAPI_EXPORT bool parse_simple_submit_response(const std::string &request, std::string &response);
			NSCAPI_EXPORT NSCAPI::nagiosReturn create_simple_query_response_unknown(std::string command, std::string msg, std::string &buffer);
			NSCAPI_EXPORT NSCAPI::nagiosReturn create_simple_query_response(std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer);
			NSCAPI_EXPORT NSCAPI::nagiosReturn create_simple_query_response(std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string &buffer);
			NSCAPI_EXPORT void append_simple_submit_request_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf = "");


			NSCAPI_EXPORT void append_simple_query_response_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf = "");
			NSCAPI_EXPORT void append_simple_exec_response_payload(Plugin::ExecuteResponseMessage::Response *payload, std::string command, int ret, std::string msg);
			NSCAPI_EXPORT void append_simple_submit_response_payload(Plugin::SubmitResponseMessage::Response *payload, std::string command, ::Plugin::Common_Result_StatusCodeType result, std::string msg);
			NSCAPI_EXPORT void append_simple_query_request_payload(Plugin::QueryRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments);
			NSCAPI_EXPORT void append_simple_exec_request_payload(Plugin::ExecuteRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments);
			NSCAPI_EXPORT void parse_simple_query_request(std::list<std::string> &args, const std::string &request);
			NSCAPI_EXPORT decoded_simple_command_data parse_simple_query_request(const char* char_command, const std::string &request);
			NSCAPI_EXPORT decoded_simple_command_data parse_simple_query_request(const std::string char_command, const std::string &request);
			NSCAPI_EXPORT decoded_simple_command_data parse_simple_query_request(const ::Plugin::QueryRequestMessage::Request &payload);
			NSCAPI_EXPORT int parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf);
			NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::list<std::string> & args, std::string &request);
			NSCAPI_EXPORT void create_simple_exec_request(const std::string &module, const std::string &command, const std::vector<std::string> & args, std::string &request);
			NSCAPI_EXPORT int parse_simple_exec_response(const std::string &response, std::list<std::string> &result);

			template<class T>
			void append_response_payloads(T &target_message, std::string &payload) {
				T source_message;
				source_message.ParseFromString(payload);
				for (int i=0;i<source_message.payload_size();++i)
					target_message.add_payload()->CopyFrom(source_message.payload(i));
			}

			NSCAPI_EXPORT int create_simple_exec_response(const std::string &command, NSCAPI::nagiosReturn ret, const std::string result, std::string &response);
			NSCAPI_EXPORT int create_simple_exec_response_unknown(std::string command, std::string result, std::string &response);
			NSCAPI_EXPORT decoded_simple_command_data parse_simple_exec_request(const std::string &request);
			NSCAPI_EXPORT decoded_simple_command_data parse_simple_exec_request(const Plugin::ExecuteRequestMessage &message);
			NSCAPI_EXPORT decoded_simple_command_data parse_simple_exec_request_payload(const Plugin::ExecuteRequestMessage::Request &payload);

			NSCAPI_EXPORT void parse_performance_data(Plugin::QueryResponseMessage::Response::Line *payload, const std::string &perf);
			NSCAPI_EXPORT std::string build_performance_data(Plugin::QueryResponseMessage::Response::Line const &payload);

			NSCAPI_EXPORT std::string extract_perf_value_as_string(const ::Plugin::Common_PerformanceData &perf);
			NSCAPI_EXPORT long long extract_perf_value_as_int(const ::Plugin::Common_PerformanceData &perf);
			NSCAPI_EXPORT std::string extract_perf_maximum_as_string(const ::Plugin::Common_PerformanceData &perf);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::QueryResponseMessage::Response* target, const ::Plugin::ExecuteResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::QueryResponseMessage::Response* target, const ::Plugin::SubmitResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::QueryResponseMessage::Response* target, const ::Plugin::QueryResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage::Response* target, const ::Plugin::ExecuteResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage::Response* target, const ::Plugin::SubmitResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::ExecuteResponseMessage::Response* target, const ::Plugin::QueryResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::SubmitResponseMessage::Response* target, const ::Plugin::ExecuteResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::SubmitResponseMessage::Response* target, const ::Plugin::SubmitResponseMessage::Response source);
			NSCAPI_EXPORT void copy_response(const std::string command, ::Plugin::SubmitResponseMessage::Response* target, const ::Plugin::QueryResponseMessage::Response source);
			
		}
	}
}