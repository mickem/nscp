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

#include <boost/math/special_functions.hpp>

#include <string>
#include <list>
#include <iostream>

#include <NSCAPI.h>
#include <charEx.h>
//#include <arrayBuffer.h>
#include <types.hpp>
#include <net/net.hpp>
#include <unicode_char.hpp>
#include <strEx.h>
#include <nscapi/settings_proxy.hpp>

#include <protobuf/plugin.pb.h>

using namespace nscp::helpers;

namespace nscapi {
	class functions {
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
				return Plugin::Common_Status_StatusType_OK;
			return Plugin::Common_Status_StatusType_PROBLEM;
		}
		static int gbp_to_status(Plugin::Common::Status::StatusType ret) {
			if (ret == Plugin::Common_Status_StatusType_OK)
				return NSCAPI::isSuccess;
			return NSCAPI::hasFailed;
		}
		static Plugin::LogEntry::Entry::Level log_to_gpb(NSCAPI::messageTypes ret) {
			if (ret == NSCAPI::critical)
				return Plugin::LogEntry_Entry_Level_LOG_CRITICAL;
			if (ret == NSCAPI::debug)
				return Plugin::LogEntry_Entry_Level_LOG_DEBUG;
			if (ret == NSCAPI::error)
				return Plugin::LogEntry_Entry_Level_LOG_ERROR;
			if (ret == NSCAPI::log)
				return Plugin::LogEntry_Entry_Level_LOG_INFO;
			if (ret == NSCAPI::warning)
				return Plugin::LogEntry_Entry_Level_LOG_WARNING;
			return Plugin::LogEntry_Entry_Level_LOG_ERROR;
		}
		static NSCAPI::messageTypes gpb_to_log(Plugin::LogEntry::Entry::Level ret) {
			if (ret == Plugin::LogEntry_Entry_Level_LOG_CRITICAL)
				return NSCAPI::critical;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_DEBUG)
				return NSCAPI::debug;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_ERROR)
				return NSCAPI::error;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_INFO)
				return NSCAPI::log;
			if (ret == Plugin::LogEntry_Entry_Level_LOG_WARNING)
				return NSCAPI::warning;
			return NSCAPI::error;
		}

		static double trim_to_double(std::wstring s) {
			std::wstring::size_type pend = s.find_first_not_of(_T("0123456789,."));
			if (pend != std::wstring::npos)
				s = s.substr(0,pend);
			strEx::replace(s, _T(","), _T("."));
			return strEx::stod(s);
		}

		struct decoded_simple_command_data {
			std::wstring command;
			std::wstring target;
			std::list<std::wstring> args;
			//std::vector<std::wstring> args_vector;
		};



		static void create_simple_header(Plugin::Common::Header* hdr)  {
			hdr->set_version(Plugin::Common_Version_VERSION_1);
			hdr->set_max_supported_version(Plugin::Common_Version_VERSION_1);
			// @todo add additional fields here!
		}

		//////////////////////////////////////////////////////////////////////////

		struct destination_container {
			std::string id;
			std::string host;
			std::string address;
			std::string protocol;
			std::string comment;
			std::list<std::string> tags;
			typedef std::map<std::string,std::string> data_map;
			data_map data;

			net::url get_url(unsigned int port = 80) {
				return net::parse(address, port);
			}

			std::string to_string() {
				std::stringstream ss;
				ss << "id: " << id;
				ss << ", host: " << host;
				ss << ", address: " << address;
				ss << ", protocol: " << protocol;
				ss << ", comment: " << comment;
				int i=0;
				BOOST_FOREACH(std::string a, tags) {
					ss << ", tags[" << i++ << "]: " << a;
				}
				BOOST_FOREACH(const data_map::value_type &kvp, data) {
					ss << ", data[" << kvp.first << "]: " << kvp.second;
				}
				return ss.str();
			}

			void import(const destination_container &other) {
				if (!other.id.empty())
					id = other.id;
				if (!other.host.empty())
					host = other.host;
				if (!other.address.empty())
					address = other.address;
				if (!other.protocol.empty())
					protocol = other.protocol;
				if (!other.comment.empty())
					comment = other.comment;
				BOOST_FOREACH(const std::string &t, other.tags) {
					tags.push_back(t);
				}
				BOOST_FOREACH(const data_map::value_type &kvp, other.data) {
					data[kvp.first] = kvp.second;
				}
			}
		};

		static void add_host(Plugin::Common::Header* hdr, const destination_container &dst)  {
			::Plugin::Common::Host *host = hdr->add_hosts();
			if (!dst.id.empty())
				host->set_id(dst.id);
			if (!dst.host.empty())
				host->set_host(dst.host);
			if (!dst.address.empty())
				host->set_address(dst.address);
			if (!dst.protocol.empty())
				host->set_protocol(dst.protocol);
			if (!dst.comment.empty())
				host->set_comment(dst.comment);
			BOOST_FOREACH(const std::string &t, dst.tags) {
				host->add_tags(t);
			}
			BOOST_FOREACH(const destination_container::data_map::value_type &kvp, dst.data) {
				::Plugin::Common_KeyValue* x = host->add_metadata();
				x->set_key(kvp.first);
				x->set_value(kvp.second);
			}
		}

		static bool parse_destination(const ::Plugin::Common_Header &header, const std::string tag, destination_container &data, const bool expand_meta = false) {
			for (int i=0;i<header.hosts_size();++i) {
				const ::Plugin::Common::Host &host = header.hosts(i);
				if (host.id() == tag) {
					data.id = tag;
					if (!host.host().empty())
						data.host = host.host();
					if (!host.address().empty())
						data.address = host.address();
					if (!host.protocol().empty())
						data.protocol = host.protocol();
					if (!host.comment().empty())
						data.comment = host.comment();
					if (expand_meta) {
						for(int j=0;j<host.tags_size(); ++j) {
							data.tags.push_back(host.tags(j));
						}
						for(int j=0;j<host.metadata_size(); ++j) {
							data.data[host.metadata(j).key()] = host.metadata(j).value();
						}
					}
					return true;
				}
			}
			return false;
		}

		//////////////////////////////////////////////////////////////////////////

		static void make_submit_from_query(std::string &message, const std::wstring channel, const std::wstring alias = _T("")) {
			Plugin::QueryResponseMessage response;
			response.ParseFromString(message);
			Plugin::SubmitRequestMessage request;
			request.mutable_header()->CopyFrom(response.header());
			request.set_channel(to_string(channel));
			for (int i=0;i<response.payload_size();++i) {
				request.add_payload()->CopyFrom(response.payload(i));
				if (!alias.empty())
					request.mutable_payload(i)->set_alias(to_string(alias));
			}
			message = request.SerializeAsString();
		}

		static void create_simple_query_request(std::wstring command, std::vector<std::wstring> arguments, std::string &buffer) {
			Plugin::QueryRequestMessage message;
			create_simple_header(message.mutable_header());

			Plugin::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(to_string(command));

			BOOST_FOREACH(std::wstring s, arguments)
				payload->add_arguments(to_string(s));

			message.SerializeToString(&buffer);
		}

		static void create_simple_submit_request(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf, std::string &buffer) {
			Plugin::SubmitRequestMessage message;
			create_simple_header(message.mutable_header());
			message.set_channel(to_string(channel));

			Plugin::QueryResponseMessage::Response *payload = message.add_payload();
			payload->set_command(to_string(command));
			payload->set_message(to_string(msg));
			payload->set_result(nagios_status_to_gpb(ret));
			if (!perf.empty())
				parse_performance_data(payload, perf);

			message.SerializeToString(&buffer);
		}
		static void create_simple_submit_response(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::string &buffer) {
			Plugin::SubmitResponseMessage message;
			create_simple_header(message.mutable_header());
			//message.set_channel(to_string(channel));

			Plugin::SubmitResponseMessage::Response *payload = message.add_payload();
			payload->set_command(utf8::cvt<std::string>(command));
			payload->mutable_status()->set_message(to_string(msg));
			payload->mutable_status()->set_status(status_to_gpb(ret));
			message.SerializeToString(&buffer);
		}
		static NSCAPI::errorReturn parse_simple_submit_request(const std::string &request, std::wstring &source, std::wstring &command, std::wstring &msg, std::wstring &perf) {
			Plugin::SubmitRequestMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				throw nscapi_exception(_T("Whoops, invalid payload size (for now)"));
			}
			Plugin::QueryResponseMessage::Response payload = message.payload().Get(0);
			source = utf8::cvt<std::wstring>(payload.source());
			command = utf8::cvt<std::wstring>(payload.command());
			msg = utf8::cvt<std::wstring>(payload.message());
			perf = utf8::cvt<std::wstring>(build_performance_data(payload));
			return gbp_to_nagios_status(payload.result());
		}
		static NSCAPI::errorReturn parse_simple_submit_request_payload(const Plugin::QueryResponseMessage::Response &payload, std::wstring &alias, std::wstring &command, std::wstring &msg, std::wstring &perf) {
			alias = utf8::cvt<std::wstring>(payload.alias());
			command = utf8::cvt<std::wstring>(payload.command());
			msg = utf8::cvt<std::wstring>(payload.message());
			perf = utf8::cvt<std::wstring>(build_performance_data(payload));
			return gbp_to_nagios_status(payload.result());
		}
		static NSCAPI::errorReturn parse_simple_submit_response(const std::string &request, std::wstring response) {
			Plugin::SubmitResponseMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				throw nscapi_exception(_T("Whoops, invalid payload size (for now)"));
			}
			::Plugin::SubmitResponseMessage::Response payload = message.payload().Get(0);
			response = to_wstring(payload.mutable_status()->message());
			return gbp_to_status(payload.mutable_status()->status());
		}
		static NSCAPI::errorReturn parse_simple_submit_response(const std::string &request, std::string response) {
			Plugin::SubmitResponseMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				throw nscapi_exception(_T("Whoops, invalid payload size (for now)"));
			}
			::Plugin::SubmitResponseMessage::Response payload = message.payload().Get(0);
			response = payload.mutable_status()->message();
			return gbp_to_status(payload.mutable_status()->status());
		}
		/*
		static void create_simple_submit_response(std::wstring msg, int status, std::string &buffer) {
			Plugin::SubmitResponseMessage message;
			create_simple_header(message.mutable_header());

			Plugin::SubmitResponseMessage::Response *payload = message.add_payload();
			payload->mutable_status()->set_message(to_string(msg));
			payload->mutable_status()->set_status(status_to_gpb(status));

			message.SerializeToString(&buffer);
		}*/

		static void create_simple_query_request(std::wstring command, std::list<std::wstring> arguments, std::string &buffer) {
			Plugin::QueryRequestMessage message;
			create_simple_header(message.mutable_header());

			Plugin::QueryRequestMessage::Request *payload = message.add_payload();
			payload->set_command(to_string(command));

			BOOST_FOREACH(std::wstring s, arguments)
				payload->add_arguments(to_string(s));

			message.SerializeToString(&buffer);
		}
		static NSCAPI::nagiosReturn create_simple_query_response_unknown(std::wstring command, std::wstring msg, std::wstring perf, std::string &buffer) {
			create_simple_query_response(command, NSCAPI::returnUNKNOWN, msg, perf, buffer);
			return NSCAPI::returnUNKNOWN;
		}

		static void create_simple_query_response(std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf, std::string &buffer) {
			Plugin::QueryResponseMessage message;
			create_simple_header(message.mutable_header());

			append_simple_query_response_payload(message.add_payload(), command, ret, msg, perf);

			message.SerializeToString(&buffer);
		}
		static void append_simple_query_response_payload(Plugin::QueryResponseMessage::Response *payload, std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf) {
			payload->set_command(to_string(command));
			payload->set_message(to_string(msg));
			payload->set_result(nagios_status_to_gpb(ret));
			if (!perf.empty())
				parse_performance_data(payload, perf);
		}

		static decoded_simple_command_data parse_simple_query_request(const wchar_t* char_command, const std::string &request) {
			decoded_simple_command_data data;

			data.command = char_command;
			Plugin::QueryRequestMessage message;
			message.ParseFromString(request);

			if (message.payload_size() != 1) {
				throw nscapi_exception(_T("Whoops, invalid payload size (for now)"));
			}
			::Plugin::QueryRequestMessage::Request payload = message.payload().Get(0);
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(to_wstring(payload.arguments(i)));
			}
			return data;
		}

		static int parse_simple_query_response(const std::string &response, std::wstring &msg, std::wstring &perf) {
			Plugin::QueryResponseMessage message;
			message.ParseFromString(response);


			if (message.payload_size() == 0) {
				return NSCAPI::returnUNKNOWN;
			} else if (message.payload_size() > 1) {
				throw nscapi_exception(_T("Whoops, invalid payload size (for now)"));
			}

			Plugin::QueryResponseMessage::Response payload = message.payload().Get(0);
			msg = utf8::cvt<std::wstring>(payload.message());
			perf = utf8::cvt<std::wstring>(build_performance_data(payload));
			return gbp_to_nagios_status(payload.result());
		}


		//////////////////////////////////////////////////////////////////////////

		static void create_simple_exec_request(const std::wstring &command, const std::list<std::wstring> & args, std::string &request) {
			
			Plugin::ExecuteRequestMessage message;
			create_simple_header(message.mutable_header());

			Plugin::ExecuteRequestMessage::Request *payload = message.add_payload();
			payload->set_command(to_string(command));

			BOOST_FOREACH(std::wstring s, args)
				payload->add_arguments(to_string(s));

			message.SerializeToString(&request);
		}
		static void create_simple_exec_request(const std::wstring &command, const std::vector<std::wstring> & args, std::string &request) {

			Plugin::ExecuteRequestMessage message;
			create_simple_header(message.mutable_header());

			Plugin::ExecuteRequestMessage::Request *payload = message.add_payload();
			payload->set_command(to_string(command));

			BOOST_FOREACH(std::wstring s, args)
				payload->add_arguments(to_string(s));

			message.SerializeToString(&request);
		}
		static void parse_simple_exec_result(const std::string &response, std::list<std::wstring> &result) {
			Plugin::ExecuteResponseMessage message;
			message.ParseFromString(response);

			for (int i=0;i<message.payload_size(); i++) {
				result.push_back(utf8::cvt<std::wstring>(message.payload(i).message()));
			}
		}

		static void create_simple_exec_response(std::wstring command, NSCAPI::nagiosReturn ret, std::wstring result, std::string &response) {
			Plugin::ExecuteResponseMessage message;
			create_simple_header(message.mutable_header());

			Plugin::ExecuteResponseMessage::Response *payload = message.add_payload();
			payload->set_command(to_string(command));
			payload->set_message(to_string(result));

			payload->set_result(nagios_status_to_gpb(ret));
			message.SerializeToString(&response);
		}
		static decoded_simple_command_data parse_simple_exec_request(const wchar_t* char_command, const std::string &request) {
			decoded_simple_command_data data;

			data.command = char_command;
			Plugin::ExecuteRequestMessage message;
			message.ParseFromString(request);
			if (message.has_header())
				data.target = utf8::cvt<std::wstring>(message.header().recipient_id());

			if (message.payload_size() != 1) {
				throw nscapi_exception(_T("Whoops, invalid payload size (for now)"));
			}
			Plugin::ExecuteRequestMessage::Request payload = message.payload().Get(0);
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(to_wstring(payload.arguments(i)));
			}
			return data;
		}


		//////////////////////////////////////////////////////////////////////////

		static void parse_performance_data(Plugin::QueryResponseMessage::Response *payload, std::wstring &perf) {
			boost::tokenizer<boost::escaped_list_separator<wchar_t>, std::wstring::const_iterator, std::wstring> tok(perf, boost::escaped_list_separator<wchar_t>(L'\\', L' ', L'\''));
			BOOST_FOREACH(std::wstring s, tok) {
				if (s.size() == 0)
					break;
				strEx::splitVector items = strEx::splitV(s, _T(";"));
				if (items.size() < 1) {
					Plugin::Common::PerformanceData* perfData = payload->add_perf();
					perfData->set_type(Plugin::Common_DataType_STRING);
					std::pair<std::wstring,std::wstring> fitem = strEx::split(_T(""), _T("="));
					perfData->set_alias("invalid");
					Plugin::Common_PerformanceData_StringValue* stringPerfData = perfData->mutable_string_value();
					stringPerfData->set_value("invalid performance data");
					break;
				}

				Plugin::Common::PerformanceData* perfData = payload->add_perf();
				perfData->set_type(Plugin::Common_DataType_FLOAT);
				std::pair<std::wstring,std::wstring> fitem = strEx::split(items[0], _T("="));
				perfData->set_alias(to_string(fitem.first));
				Plugin::Common_PerformanceData_FloatValue* floatPerfData = perfData->mutable_float_value();

				std::wstring::size_type pend = fitem.second.find_first_not_of(_T("0123456789,."));
				if (pend == std::wstring::npos) {
					floatPerfData->set_value(trim_to_double(fitem.second.c_str()));
				} else {
					floatPerfData->set_value(trim_to_double(fitem.second.substr(0,pend).c_str()));
					floatPerfData->set_unit(to_string(fitem.second.substr(pend)));
				}
				if (items.size() > 2) {
					floatPerfData->set_warning(trim_to_double(items[1]));
					floatPerfData->set_critical(trim_to_double(items[2]));
				}
				if (items.size() >= 5) {
					floatPerfData->set_minimum(trim_to_double(items[3]));
					floatPerfData->set_maximum(trim_to_double(items[4]));
				}
			}
//			std::wcout << _T("Converting performance data") << perf << _T(" -- ") << utf8::cvt<std::wstring>(build_performance_data(*resp)) << std::endl;
		}
		static std::string build_performance_data(Plugin::QueryResponseMessage::Response const &payload) {
			std::stringstream ss;
			ss.precision(5);

			bool first = true;
			for (int i=0;i<payload.perf_size();i++) {
				Plugin::Common::PerformanceData perfData = payload.perf(i);
				if (!first)
					ss << " ";
				first = false;
				ss << '\'' << perfData.alias() << "'=";
				if (perfData.has_float_value()) {
					Plugin::Common_PerformanceData_FloatValue fval = perfData.float_value();

					ss << fval.value();
					if (fval.has_unit())
						ss << fval.unit();
					if (!fval.has_warning())
						continue;
					ss << ";" << fval.warning();
					if (!fval.has_critical())	
						continue;
					ss << ";" << fval.critical();
					if (!fval.has_minimum())
						continue;
					ss << ";" << fval.minimum();
					if (!fval.has_maximum())
						continue;
					ss << ";" << fval.maximum();
				}
			}
			return ss.str();
		}
	};
}