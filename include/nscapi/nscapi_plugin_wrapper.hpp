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
#include <nscapi/settings_proxy.hpp>
#include <nscapi/functions.hpp>

#include "../libs/protobuf/plugin.proto.h"
#include "../libs/protobuf/log.proto.h"

using namespace nscp::helpers;

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



	namespace impl {

		class simple_plugin {
		public:
			inline nscapi::core_wrapper* get_core() {
				return nscapi::plugin_singleton->get_core();
			}
			inline boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy() {
				return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(nscapi::plugin_singleton->get_core()));
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
				nscapi::functions::decoded_simple_command_data data = nscapi::functions::process_simple_command_request(char_command, request);
				std::wstring msg, perf;
				NSCAPI::nagiosReturn ret = handleCommand(data.command, data.args, msg, perf);
				return nscapi::functions::process_simple_command_result(data.command, ret, msg, perf, response);
			}

			virtual NSCAPI::nagiosReturn handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) = 0;
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
