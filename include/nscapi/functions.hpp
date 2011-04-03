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
#include <arrayBuffer.h>
#include <types.hpp>

#include <unicode_char.hpp>
#include <strEx.h>
#include <nscapi/settings_proxy.hpp>

#include "../libs/protobuf/plugin.proto.h"
#include "../libs/protobuf/log.proto.h"

using namespace nscp::helpers;

namespace nscapi {

	class functions {
	public:
		static PluginCommand::Response_Code nagios_to_gpb(int ret) {
			if (ret == NSCAPI::returnOK)
				return PluginCommand::Response_Code_OK;
			if (ret == NSCAPI::returnWARN)
				return PluginCommand::Response_Code_WARNING;
			if (ret == NSCAPI::returnCRIT)
				return PluginCommand::Response_Code_CRITCAL;
			return PluginCommand::Response_Code_UNKNOWN;
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
			std::list<std::wstring> args;
		};
		static decoded_simple_command_data process_simple_command_request(const wchar_t* char_command, const std::string &request) {
			decoded_simple_command_data data;

			data.command = char_command;
			PluginCommand::RequestMessage request_message;
			request_message.ParseFromString(request);

			if (request_message.payload_size() != 1) {
				throw exception("Whoops, invalid payload size (for now)");
			}
			::PluginCommand::Request payload = request_message.payload().Get(0);
			for (int i=0;i<payload.arguments_size();i++) {
				data.args.push_back(to_wstring(payload.arguments(i)));
			}
			return data;
		}
		static NSCAPI::nagiosReturn process_simple_command_result(std::wstring command, NSCAPI::nagiosReturn ret, std::wstring msg, std::wstring perf, std::string &response) {
			PluginCommand::ResponseMessage response_message;
			::PluginCommand::Header* hdr = response_message.mutable_header();

			hdr->set_type(PluginCommand::Header_Type_RESPONSE);
			hdr->set_version(PluginCommand::Header_Version_VERSION_1);

			PluginCommand::Response *resp = response_message.add_payload();
			resp->set_command(to_string(command));
			resp->set_message(to_string(msg));
			parse_performance_data(resp, perf);

			resp->set_version(PluginCommand::Response_Version_VERSION_1);
			resp->set_result(nagios_to_gpb(ret));
			response_message.SerializeToString(&response);
			return ret;
		}
		static void parse_performance_data(PluginCommand::Response *resp, std::wstring &perf) {

			boost::tokenizer<boost::escaped_list_separator<wchar_t>, std::wstring::const_iterator, std::wstring> tok(perf, boost::escaped_list_separator<wchar_t>(L'\\', L' ', L'\''));
			BOOST_FOREACH(std::wstring s, tok) {
				strEx::splitVector items = strEx::splitV(s, _T(";"));
				if (items.size() < 3)
					break;

				::PluginCommand::PerformanceData* perfData = resp->add_perf();
				perfData->set_type(PluginCommand::PerformanceData_Type_FLOAT);
				std::pair<std::wstring,std::wstring> fitem = strEx::split(items[0], _T("="));
				perfData->set_alias(to_string(fitem.first));
				::PluginCommand::PerformanceData_FloatValue* floatPerfData = perfData->mutable_float_value();

				std::wstring::size_type pend = fitem.second.find_first_not_of(_T("0123456789,."));
				if (pend == std::wstring::npos) {
					floatPerfData->set_value(trim_to_double(fitem.second.c_str()));
				} else {
					floatPerfData->set_value(trim_to_double(fitem.second.substr(0,pend).c_str()));
					floatPerfData->set_unit(to_string(fitem.second.substr(pend)));
				}
				floatPerfData->set_warning(trim_to_double(items[1]));
				floatPerfData->set_critical(trim_to_double(items[2]));
				if (items.size() >= 5) {
					floatPerfData->set_minimum(trim_to_double(items[3]));
					floatPerfData->set_maximum(trim_to_double(items[4]));
				}
			}
		}
		static std::string build_performance_data(::PluginCommand::Response const &payload) {
			std::stringstream ss;
			ss.precision(5);

			bool first = true;
			for (int i=0;i<payload.perf_size();i++) {
				::PluginCommand::PerformanceData perfData = payload.perf(i);
				if (!first)
					ss << " ";
				first = false;
				ss << '\'' << perfData.alias() << "'=";
				if (perfData.has_float_value()) {
					::PluginCommand::PerformanceData_FloatValue fval = perfData.float_value();

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