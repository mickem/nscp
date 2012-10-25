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

#include <NSCAPI.h>
#include <nscapi/nscapi_protobuf_types.hpp>

#include <protobuf/plugin.pb.h>

#include <strEx.h>

namespace nscapi {
	namespace protobuf {	

		class functions {
		public:

			typedef nscapi::protobuf::types::destination_container destination_container;
			typedef nscapi::protobuf::types::decoded_simple_command_data decoded_simple_command_data;
			typedef nscapi::protobuf::types::decoded_simple_command_data_utf8 decoded_simple_command_data_utf8;
		public:
			static Plugin::Common::ResultCode nagios_status_to_gpb(int ret) {
				if (ret == NSCAPI::returnOK)
					return Plugin::Common_ResultCode_OK;
				if (ret == NSCAPI::returnWARN)
					return Plugin::Common_ResultCode_WARNING;
				if (ret == NSCAPI::returnCRIT)
					return Plugin::Common_ResultCode_CRITCAL;
				return Plugin::Common_ResultCode_UNKNOWN;
			}
			static int gbp_to_nagios_status(Plugin::Common::ResultCode ret) {
				if (ret == Plugin::Common_ResultCode_OK)
					return NSCAPI::returnOK;
				if (ret == Plugin::Common_ResultCode_WARNING)
					return NSCAPI::returnWARN;
				if (ret == Plugin::Common_ResultCode_CRITCAL)
					return NSCAPI::returnCRIT;
				return NSCAPI::returnUNKNOWN;
			}
			static Plugin::Common::Status::StatusType status_to_gpb(int ret) {
				if (ret == NSCAPI::isSuccess)
					return Plugin::Common_Status_StatusType_STATUS_OK;
				return Plugin::Common_Status_StatusType_STATUS_ERROR;
			}
			static int gbp_to_status(Plugin::Common::Status::StatusType ret) {
				if (ret == Plugin::Common_Status_StatusType_STATUS_OK)
					return NSCAPI::isSuccess;
				return NSCAPI::hasFailed;
			}
			static Plugin::Common::ResultCode gbp_status_to_gbp_nagios(Plugin::Common::Status::StatusType ret) {
				if (ret == Plugin::Common_Status_StatusType_STATUS_OK)
					return Plugin::Common_ResultCode_OK;
				return Plugin::Common_ResultCode_UNKNOWN;
			}
			static Plugin::Common::Status::StatusType gbp_to_nagios_gbp_status(Plugin::Common::ResultCode ret) {
				if (ret == Plugin::Common_ResultCode_UNKNOWN||ret == Plugin::Common_ResultCode_WARNING||ret == Plugin::Common_ResultCode_CRITCAL)
					return Plugin::Common_Status_StatusType_STATUS_ERROR;
				return Plugin::Common_Status_StatusType_STATUS_OK;
			}
			
			static Plugin::LogEntry::Entry::Level log_to_gpb(NSCAPI::messageTypes ret) {
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
			static NSCAPI::messageTypes gpb_to_log(Plugin::LogEntry::Entry::Level ret) {
				if (ret == Plugin::LogEntry_Entry_Level_LOG_CRITICAL)
					return NSCAPI::log_level::critical;
				if (ret == Plugin::LogEntry_Entry_Level_LOG_DEBUG)
					return NSCAPI::log_level::debug;
				if (ret == Plugin::LogEntry_Entry_Level_LOG_ERROR)
					return NSCAPI::log_level::error;
				if (ret == Plugin::LogEntry_Entry_Level_LOG_INFO)
					return NSCAPI::log_level::info;
				if (ret == Plugin::LogEntry_Entry_Level_LOG_WARNING)
					return NSCAPI::log_level::warning;
				return NSCAPI::log_level::error;
			}


			static void create_simple_header(Plugin::Common::Header* hdr);
			static void add_host(Plugin::Common::Header* hdr, const destination_container &dst);
			static bool parse_destination(const ::Plugin::Common_Header &header, const std::string tag, destination_container &data, const bool expand_meta = false);

			static void make_submit_from_query(std::string &message, const std::wstring channel, const std::wstring alias = _T(""), const std::wstring target = _T(""));
			static void make_query_from_exec(std::string &data);
			static void make_query_from_submit(std::string &data);
			static void make_exec_from_submit(std::string &data);
			static void make_exec_from_query(std::string &data);
			static void make_return_header(::Plugin::Common_Header *target, const ::Plugin::Common_Header &source);

			static void create_simple_query_request(std::wstring command, std::vector<std::wstring> arguments, std::string &buffer);
			static void create_simple_query_request(std::wstring command, std::list<std::wstring> arguments, std::string &buffer);
			static void create_simple_query_request(std::string command, std::list<std::string> arguments, std::string &buffer);
			static void create_simple_submit_request(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf, std::string &buffer);
			static void create_simple_submit_request(std::string channel, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer);
			static void create_simple_submit_response(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::string &buffer);
			static NSCAPI::errorReturn parse_simple_submit_request(const std::string &request, std::wstring &source, std::wstring &command, std::wstring &msg, std::wstring &perf);

			static int parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::wstring &alias, std::wstring &message, std::wstring &perf);
			static int parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::wstring &alias, std::wstring &message);
			static void parse_simple_query_request_payload(const Plugin::QueryRequestMessage::Request &payload, std::wstring &alias, std::wstring &command);
			static NSCAPI::errorReturn parse_simple_submit_response(const std::string &request, std::wstring &response);
			static NSCAPI::errorReturn parse_simple_submit_response(const std::string &request, std::string response);
			static NSCAPI::nagiosReturn create_simple_query_response_unknown(std::wstring command, std::wstring msg, std::wstring perf, std::string &buffer);
			static NSCAPI::nagiosReturn create_simple_query_response_unknown(std::wstring command, std::wstring msg, std::string &buffer);
			static void create_simple_query_response(std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf, std::string &buffer);
			static void create_simple_query_response(std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf, std::string &buffer);
			static void append_simple_submit_request_payload(Plugin::QueryResponseMessage::Response *payload, std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf = _T(""));
			static void append_simple_query_response_payload(Plugin::QueryResponseMessage::Response *payload, std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf);


			static void append_simple_query_response_payload(Plugin::QueryResponseMessage::Response *payload, std::string command, NSCAPI::nagiosReturn ret, std::string msg, std::string perf = "");
			static void append_simple_exec_response_payload(Plugin::ExecuteResponseMessage::Response *payload, std::string command, int ret, std::string msg);
			static void append_simple_submit_response_payload(Plugin::SubmitResponseMessage::Response *payload, std::string command, int ret, std::string msg);
			static void append_simple_query_request_payload(Plugin::QueryRequestMessage::Request *payload, std::wstring command, std::vector<std::wstring> arguments);
			static void append_simple_exec_request_payload(Plugin::ExecuteRequestMessage::Request *payload, std::wstring command, std::vector<std::wstring> arguments);
			static void parse_simple_query_request(std::list<std::string> &args, const std::string &request);
			static decoded_simple_command_data parse_simple_query_request(const wchar_t* char_command, const std::string &request);
			static decoded_simple_command_data parse_simple_query_request(const std::string char_command, const std::string &request);
			static decoded_simple_command_data parse_simple_query_request(const ::Plugin::QueryRequestMessage::Request &payload);
			static decoded_simple_command_data_utf8 parse_simple_query_request_utf8(const wchar_t* char_command, const std::string &request);
			static int parse_simple_query_response(const std::string &response, std::wstring &msg, std::wstring &perf);
			static int parse_simple_query_response(const std::string &response, std::string &msg, std::string &perf);
			static void create_simple_exec_request(const std::wstring &command, const std::list<std::wstring> & args, std::string &request);
			static void create_simple_exec_request(const std::string &command, const std::list<std::string> & args, std::string &request);
			static void create_simple_exec_request(const std::wstring &command, const std::vector<std::wstring> & args, std::string &request);
			static int parse_simple_exec_result(const std::string &response, std::list<std::wstring> &result);
			static void parse_simple_exec_result(const std::string &response, std::wstring &result);
			static int parse_simple_exec_response(const std::string &response, std::list<std::string> &result);

			template<class T>
			static void append_response_payloads(T &target_message, std::string &payload) {
				T source_message;
				source_message.ParseFromString(payload);
				for (int i=0;i<source_message.payload_size();++i)
					target_message.add_payload()->CopyFrom(source_message.payload(i));
			}

			template<class T>
			static int create_simple_exec_response(T command, NSCAPI::nagiosReturn ret, T result, std::string &response) {
				Plugin::ExecuteResponseMessage message;
				create_simple_header(message.mutable_header());

				Plugin::ExecuteResponseMessage::Response *payload = message.add_payload();
				payload->set_command(utf8::cvt<std::string>(command));
				payload->set_message(utf8::cvt<std::string>(result));

				payload->set_result(nagios_status_to_gpb(ret));
				message.SerializeToString(&response);
				return ret;
			}
			template<class T>
			static int create_simple_exec_response_unknown(T command, T result, std::string &response) {
				Plugin::ExecuteResponseMessage message;
				create_simple_header(message.mutable_header());

				Plugin::ExecuteResponseMessage::Response *payload = message.add_payload();
				payload->set_command(utf8::cvt<std::string>(command));
				payload->set_message(utf8::cvt<std::string>(result));

				payload->set_result(nagios_status_to_gpb(NSCAPI::returnUNKNOWN));
				message.SerializeToString(&response);
				return NSCAPI::returnUNKNOWN;
			}
			static decoded_simple_command_data parse_simple_exec_request(const wchar_t* char_command, const std::string &request);
			static decoded_simple_command_data parse_simple_exec_request(const std::wstring cmd, const Plugin::ExecuteRequestMessage &message);
			static decoded_simple_command_data parse_simple_exec_request_payload(const Plugin::ExecuteRequestMessage::Request &payload);

			static void parse_performance_data(Plugin::QueryResponseMessage::Response *payload, std::wstring &perf);
			static void parse_performance_data(Plugin::QueryResponseMessage::Response *payload, std::string &perf);
			static std::string build_performance_data(Plugin::QueryResponseMessage::Response const &payload);
		};
	}
	typedef protobuf::functions functions;
}