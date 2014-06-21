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

#include <strEx.h>

namespace nscapi {
	namespace protobuf {
		namespace functions {

			typedef nscapi::protobuf::types::destination_container destination_container;
			typedef nscapi::protobuf::types::decoded_simple_command_data decoded_simple_command_data;
			
			class settings_query {
				::Plugin::SettingsRequestMessage request_message;
				::Plugin::SettingsResponseMessage response_message;
				std::string response_buffer;
				int plugin_id;

			public:
				struct key_values {
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

			inline void set_response_good(::Plugin::QueryResponseMessage::Response &response, std::string message) {
				response.set_result(::Plugin::Common_ResultCode_OK);
				response.set_message(message);
			}
			inline void set_response_good(::Plugin::ExecuteResponseMessage::Response &response, std::string message) {
				response.set_result(::Plugin::Common_ResultCode_OK);
				response.set_message(message);
			}
			inline void set_response_good(::Plugin::SubmitResponseMessage::Response &response, std::string message) {
				response.mutable_status()->set_status(::Plugin::Common_Status_StatusType_STATUS_OK);
				response.mutable_status()->set_message(message);
			}

			inline void set_response_bad(::Plugin::QueryResponseMessage::Response &response, std::string message) {
				response.set_result(Plugin::Common_ResultCode_UNKNOWN);
				response.set_message(message);
			}
			inline void set_response_bad(::Plugin::ExecuteResponseMessage::Response &response, std::string message) {
				response.set_result(Plugin::Common_ResultCode_UNKNOWN);
				response.set_message(message);
			}
			inline void set_response_bad(::Plugin::SubmitResponseMessage::Response &response, std::string message) {
				response.mutable_status()->set_status(::Plugin::Common_Status_StatusType_STATUS_ERROR);
				response.mutable_status()->set_message(message);
			}

			Plugin::Common::ResultCode parse_nagios(const std::string &status);
			Plugin::Common::ResultCode nagios_status_to_gpb(int ret);
			int gbp_to_nagios_status(Plugin::Common::ResultCode ret);
			inline Plugin::Common::Status::StatusType status_to_gpb(int ret) {
				if (ret == NSCAPI::isSuccess)
					return Plugin::Common_Status_StatusType_STATUS_OK;
				return Plugin::Common_Status_StatusType_STATUS_ERROR;
			}
			inline int gbp_to_status(Plugin::Common::Status::StatusType ret) {
				if (ret == Plugin::Common_Status_StatusType_STATUS_OK)
					return NSCAPI::isSuccess;
				return NSCAPI::hasFailed;
			}
			inline Plugin::Common::ResultCode gbp_status_to_gbp_nagios(Plugin::Common::Status::StatusType ret) {
				if (ret == Plugin::Common_Status_StatusType_STATUS_OK)
					return Plugin::Common_ResultCode_OK;
				return Plugin::Common_ResultCode_UNKNOWN;
			}
			inline Plugin::Common::Status::StatusType gbp_to_nagios_gbp_status(Plugin::Common::ResultCode ret) {
				if (ret == Plugin::Common_ResultCode_UNKNOWN||ret == Plugin::Common_ResultCode_WARNING||ret == Plugin::Common_ResultCode_CRITCAL)
					return Plugin::Common_Status_StatusType_STATUS_ERROR;
				return Plugin::Common_Status_StatusType_STATUS_OK;
			}
			
			inline Plugin::LogEntry::Entry::Level log_to_gpb(NSCAPI::messageTypes ret) {
				if (ret == NSCAPI::log_level::critical)
					return Plugin::LogEntry_Entry_Level_LOG_CRITICAL;
				if (ret == NSCAPI::log_level::debug)
					return Plugin::LogEntry_Entry_Level_LOG_DEBUG;
				if (ret == NSCAPI::log_level::error)
					return Plugin::LogEntry_Entry_Level_LOG_ERROR;
				if (ret == NSCAPI::log_level::info)
					return Plugin::LogEntry_Entry_Level_LOG_INFO;
				if (ret == NSCAPI::log_level::warning)
					return Plugin::LogEntry_Entry_Level_LOG_WARNING;
				return Plugin::LogEntry_Entry_Level_LOG_ERROR;
			}
			NSCAPI::messageTypes gpb_to_log(Plugin::LogEntry::Entry::Level ret);


			void create_simple_header(Plugin::Common::Header* hdr);
			void add_host(Plugin::Common::Header* hdr, const destination_container &dst);
			bool parse_destination(const ::Plugin::Common_Header &header, const std::string tag, destination_container &data, const bool expand_meta = false);

			void make_submit_from_query(std::string &message, const std::string channel, const std::string alias = "", const std::string target = "", const std::string source = "");
			void make_query_from_exec(std::string &data);
			void make_query_from_submit(std::string &data);
			void make_exec_from_submit(std::string &data);
			void make_exec_from_query(std::string &data);
			void make_return_header(::Plugin::Common_Header *target, const ::Plugin::Common_Header &source);

			void create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer);
			void create_simple_query_request(std::string command, std::vector<std::string> arguments, std::string &buffer);
			void create_simple_submit_request(std::string channel, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer);
			void create_simple_submit_response(const std::string channel, const std::string command, const NSCAPI::nagiosReturn ret, const std::string msg, std::string &buffer);

			int parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::string &alias, std::string &message, std::string &perf);
 			int parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::wstring &alias, std::wstring &message, std::wstring &perf);
 			int parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::wstring &alias, std::wstring &message);
			NSCAPI::errorReturn parse_simple_submit_response(const std::string &request, std::string &response);
			NSCAPI::nagiosReturn create_simple_query_response_unknown(std::string command, std::string msg, std::string &buffer);
			NSCAPI::nagiosReturn create_simple_query_response(std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer);
			NSCAPI::nagiosReturn create_simple_query_response(std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string &buffer);
			void append_simple_submit_request_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf = "");


			void append_simple_query_response_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf = "");
			void append_simple_exec_response_payload(Plugin::ExecuteResponseMessage::Response *payload, std::string command, int ret, std::string msg);
			void append_simple_submit_response_payload(Plugin::SubmitResponseMessage::Response *payload, std::string command, int ret, std::string msg);
			void append_simple_query_request_payload(Plugin::QueryRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments);
			void append_simple_exec_request_payload(Plugin::ExecuteRequestMessage::Request *payload, std::string command, std::vector<std::string> arguments);
			void parse_simple_query_request(std::list<std::string> &args, const std::string &request);
			decoded_simple_command_data parse_simple_query_request(const char* char_command, const std::string &request);
			decoded_simple_command_data parse_simple_query_request(const std::string char_command, const std::string &request);
			decoded_simple_command_data parse_simple_query_request(const ::Plugin::QueryRequestMessage::Request &payload);
			int parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf);
			void create_simple_exec_request(const std::string &command, const std::list<std::string> & args, std::string &request);
			void create_simple_exec_request(const std::string &command, const std::vector<std::string> & args, std::string &request);
			int parse_simple_exec_response(const std::string &response, std::list<std::string> &result);

			template<class T>
			void append_response_payloads(T &target_message, std::string &payload) {
				T source_message;
				source_message.ParseFromString(payload);
				for (int i=0;i<source_message.payload_size();++i)
					target_message.add_payload()->CopyFrom(source_message.payload(i));
			}

			int create_simple_exec_response(const std::string &command, NSCAPI::nagiosReturn ret, const std::string result, std::string &response);
			int create_simple_exec_response_unknown(std::string command, std::string result, std::string &response);
			decoded_simple_command_data parse_simple_exec_request(const std::string &request);
			decoded_simple_command_data parse_simple_exec_request(const Plugin::ExecuteRequestMessage &message);
			decoded_simple_command_data parse_simple_exec_request_payload(const Plugin::ExecuteRequestMessage::Request &payload);

			void parse_performance_data(Plugin::QueryResponseMessage::Response *payload, const std::string &perf);
			std::string build_performance_data(Plugin::QueryResponseMessage::Response const &payload);

			std::string extract_perf_value_as_string(const ::Plugin::Common_PerformanceData &perf);
			long long extract_perf_value_as_int(const ::Plugin::Common_PerformanceData &perf);
			std::string extract_perf_maximum_as_string(const ::Plugin::Common_PerformanceData &perf);
		}
	}
}