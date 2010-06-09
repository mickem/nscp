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

	namespace impl {

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
					std::wstring msg = to_wstring(payload.message()), perf;
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
				NSCAPI::nagiosReturn ret = handleCommand(command, args, msg, perf);

				PluginCommand::ResponseMessage response_message;
				::PluginCommand::Header* hdr = response_message.mutable_header();

				hdr->set_type(PluginCommand::Header_Type_RESPONSE);
				hdr->set_version(PluginCommand::Header_Version_VERSION_1);

				PluginCommand::Response *resp = response_message.add_payload();
				resp->set_command(to_string(command));
				resp->set_message(to_string(msg));
				resp->set_version(PluginCommand::Response_Version_VERSION_1);
				if (ret == NSCAPI::returnOK)
					resp->set_result(PluginCommand::Response_Code_OK);
				else if (ret == NSCAPI::returnWARN)
					resp->set_result(PluginCommand::Response_Code_WARNING);
				else if (ret == NSCAPI::returnCRIT)
					resp->set_result(PluginCommand::Response_Code_CRITCAL);
				else 
					resp->set_result(PluginCommand::Response_Code_UNKNOWN);

				response_message.SerializeToString(&response);
				return ret;
			}

			virtual NSCAPI::nagiosReturn handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) = 0;
		};
	}
};
