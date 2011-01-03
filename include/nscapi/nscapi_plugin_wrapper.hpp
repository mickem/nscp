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
#include <iostream>

#include <NSCAPI.h>
#include <charEx.h>
#include <arrayBuffer.h>
#include <types.hpp>

#include <unicode_char.hpp>
#include <strEx.h>

#include "../libs/protobuf/plugin.proto.h"
#include "../libs/protobuf/log.proto.h"

using namespace nscp::helpers;

#ifdef WIN32
//#include <windows.h>
#endif


namespace nscapi {
	class plugin_wrapper {
	public:
		struct module_version {
			int major;
			int minor;
			int revision;
		};

		plugin_wrapper() : hModule_(NULL) {}
		int wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode);

#ifdef WIN32
	private:
		HINSTANCE hModule_;
	public:
		int wrapDllMain(HANDLE hModule, DWORD ul_reason_for_call);
		HINSTANCE getModule() {
			hModule_;
		}
#else
		void* hModule_;
#endif
		int wrapModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f);;
		NSCAPI::errorReturn wrapGetModuleName(wchar_t* buf, unsigned int buflen, std::wstring str);
		int wrapLoadModule(bool success);
		NSCAPI::errorReturn wrapGetModuleVersion(int *major, int *minor, int *revision, module_version version);
		NSCAPI::boolReturn wrapHasCommandHandler(bool has);
		NSCAPI::boolReturn wrapHasMessageHandler(bool has);
		NSCAPI::boolReturn wrapHasNotificationHandler(bool has);
		NSCAPI::nagiosReturn wrapHandleNotification(NSCAPI::nagiosReturn retResult);
		int wrapUnloadModule(bool success);
		NSCAPI::nagiosReturn wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::string &reply, char **reply_buffer, unsigned int *size);
		void wrapDeleteBuffer(char**buffer);

	};
	class core_wrapper;
	class helper_singleton {
		core_wrapper* core_;
		plugin_wrapper *plugin_;
	public:
		helper_singleton();
		core_wrapper* get_core() {
			return core_;
		}
		plugin_wrapper* get_plugin() {
			return plugin_;
		}
	};

	extern helper_singleton* plugin_singleton;

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

		static void parse_performance_data(PluginCommand::Response *resp, std::wstring &perf) {
			strEx::splitList items = strEx::splitEx(perf, _T(" "));
			for (strEx::splitList::const_iterator cit = items.begin(); cit != items.end(); ++cit) {
				strEx::splitVector items = strEx::splitV(*cit, _T(";"));
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
		static std::string build_performance_data(::PluginCommand::Response &payload) {
			std::string ret;
			for (int i=0;i<payload.perf_size();i++) {
				::PluginCommand::PerformanceData perfData = payload.perf(i);
				if (!ret.empty())
					ret += " ";
				ret += perfData.alias() + "=";
				if (perfData.has_float_value()) {
					::PluginCommand::PerformanceData_FloatValue fval = perfData.float_value();
					ret += to_string(fval.value());
					if (fval.has_unit())
						ret += fval.unit();
					if (!fval.has_warning())	continue;
					ret += ";" + to_string(fval.warning());
					if (!fval.has_critical())	continue;
					ret += ";" + to_string(fval.critical());
					if (!fval.has_minimum())	continue;
					ret += ";" + to_string(fval.minimum());
					if (!fval.has_maximum())	continue;
					ret += ";" + to_string(fval.maximum());
				}
			}
			return ret;
		}
	};

	namespace impl {

		class simple_plugin {
		public:
			inline nscapi::core_wrapper* get_core() {
				return nscapi::plugin_singleton->get_core();
			}
		};

		class SimpleNotificationHandler {
		public:
			NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, const wchar_t* command, NSCAPI::nagiosReturn code, std::string result) {
				try {
					PluginCommand::ResponseMessage message;
					message.ParseFromString(result);
					if (message.payload_size() != 1) {
						//NSC_LOG_ERROR_STD(_T("Unsupported payload size: ") + to_wstring(request_message.payload_size()));
						return NSCAPI::returnIgnored;
					}

					::PluginCommand::Response payload = message.payload().Get(0);
					
					std::list<std::wstring> args;
					for (int i=0;i<payload.arguments_size();i++) {
						args.push_back(to_wstring(payload.arguments(i)));
					}
					std::wstring msg = to_wstring(payload.message());
					std::wstring perf = to_wstring(::nscapi::functions::build_performance_data(payload));
					NSCAPI::nagiosReturn ret = handleSimpleNotification(channel, command, code, msg, perf);
				} catch (std::exception &e) {
					std::cout << "Failed to parse data from: " << strEx::strip_hex(result) << e.what() <<  std::endl;;
				} catch (...) {
					std::cout << "Failed to parse data from: " << strEx::strip_hex(result) << std::endl;;
				}

				return -1;
			}
			virtual NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) = 0;

		};

		class SimpleCommand {

		public:
			NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {

				std::wstring command = char_command;
				PluginCommand::RequestMessage request_message;
				request_message.ParseFromString(request);

				if (request_message.payload_size() != 1) {
					return NSCAPI::returnIgnored;
				}
				::PluginCommand::Request payload = request_message.payload().Get(0);
				std::list<std::wstring> args;
				for (int i=0;i<payload.arguments_size();i++) {
					args.push_back(to_wstring(payload.arguments(i)));
				}
				std::wstring msg, perf;
				NSCAPI::nagiosReturn ret = handleCommand(command.c_str(), args, msg, perf);

				PluginCommand::ResponseMessage response_message;
				::PluginCommand::Header* hdr = response_message.mutable_header();

				hdr->set_type(PluginCommand::Header_Type_RESPONSE);
				hdr->set_version(PluginCommand::Header_Version_VERSION_1);

				PluginCommand::Response *resp = response_message.add_payload();
				resp->set_command(to_string(command));
				resp->set_message(to_string(msg));
				::nscapi::functions::parse_performance_data(resp, perf);

				resp->set_version(PluginCommand::Response_Version_VERSION_1);
				resp->set_result(nscapi::functions::nagios_to_gpb(ret));
				response_message.SerializeToString(&response);
				return ret;
			}

			virtual NSCAPI::nagiosReturn handleCommand(const strEx::wci_string command, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) = 0;
		};


		class simple_log_handler {
		public:
			void handleMessageRAW(std::string data) {
				try {
					LogMessage::LogMessage message;
					message.ParseFromString(data);

					for (int i=0;i<message.message_size();i++) {
						LogMessage::Message msg = message.message(i);
						handleMessage(msg.level(), msg.file(), msg.line(), msg.message());
					}
				} catch (std::exception &e) {
					std::cout << "Failed to parse data from: " << strEx::strip_hex(data) << e.what() <<  std::endl;;
				} catch (...) {
					std::cout << "Failed to parse data from: " << strEx::strip_hex(data) << std::endl;;
				}
			}

			virtual void handleMessage(int msgType, const std::string file, int line, std::string message) = 0;

		};


		class CommandImpl {

		public:
			NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {

				std::wstring command = char_command;
				PluginCommand::RequestMessage request_message;
				request_message.ParseFromString(request);

				if (request_message.payload_size() != 1) {
					return NSCAPI::returnIgnored;
				}
				::PluginCommand::Request req_payload = request_message.payload().Get(0);

				PluginCommand::ResponseMessage response_message;
				::PluginCommand::Header* hdr = response_message.mutable_header();

				hdr->set_type(PluginCommand::Header_Type_RESPONSE);
				hdr->set_version(PluginCommand::Header_Version_VERSION_1);

				PluginCommand::Response *resp_payload = response_message.add_payload();

				handleCommand(command, &req_payload, resp_payload);


				resp_payload->set_version(PluginCommand::Response_Version_VERSION_1);
				response_message.SerializeToString(&response);

				return NSCAPI::returnOK;
			}

			virtual void handleCommand(std::wstring command, PluginCommand::Request *request, PluginCommand::Response *response) = 0;
		};
	}
};
