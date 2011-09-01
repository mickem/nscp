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
#include <vector>

#include <boost/make_shared.hpp>
#include <unicode_char.hpp>

#include <NSCAPI.h>
#include <nscapi/settings_proxy.hpp>


namespace PluginCommand {
	class Request;
	class Response;

};
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
		NSCAPI::boolReturn wrapHasRoutingHandler(bool has);
		NSCAPI::boolReturn wrapHasNotificationHandler(bool has);
		NSCAPI::nagiosReturn wrapHandleNotification(NSCAPI::nagiosReturn retResult);
		int wrapUnloadModule(bool success);
		NSCAPI::nagiosReturn wrapHandleCommand(NSCAPI::nagiosReturn retResult, const std::string &reply, char **reply_buffer, unsigned int *size);
		NSCAPI::nagiosReturn wrapCommandLineExec(NSCAPI::nagiosReturn retResult, const std::string &reply, char **reply_buffer, unsigned int *size);
		NSCAPI::nagiosReturn wrapRouteMessage(NSCAPI::nagiosReturn retResult);
		
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
			NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, const wchar_t* command, NSCAPI::nagiosReturn code, std::string result);
			virtual NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) = 0;

		};

		class simple_command {
		public:
			NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
			virtual NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf) = 0;
		};
		class simple_command_line_exec {
		public:
			NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);
			virtual NSCAPI::nagiosReturn commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) = 0;
		};


		class simple_log_handler {
		public:
			void handleMessageRAW(std::string data);
			virtual void handleMessage(int msgType, const std::string file, int line, std::string message) = 0;
		};

/*
		class CommandImpl {
		public:
			NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
			virtual void handleCommand(std::wstring command, PluginCommand::Request *request, PluginCommand::Response *response) = 0;
		};
		*/
	}
};
