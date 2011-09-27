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
#include <map>

#include <boost/make_shared.hpp>
#include <unicode_char.hpp>

#include <NSCAPI.h>
#include <nscapi/settings_proxy.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper.hpp>


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
		struct simple_plugin {
			int id_;
			inline nscapi::core_wrapper* get_core() {
				return plugin_singleton->get_core();
			}
			inline unsigned int get_id() {
				return id_;
			}
			inline void set_id(unsigned int id) {
				id_ = id;
			}
			inline boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy() {
				return boost::shared_ptr<nscapi::settings_proxy>(new nscapi::settings_proxy(get_core()));
			}
			void register_command(std::wstring command, std::wstring description) {
				get_core()->registerCommand(get_id(), command, description);
			}

		};

		class simple_submission_handler {
		public:
			NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
			virtual NSCAPI::nagiosReturn handleSimpleNotification(const std::wstring channel, const std::wstring source, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) = 0;

		};

		class simple_command_handler {
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

	};



	template<class impl_type>
	struct plugin_instance_data {
		typedef std::map<unsigned int, boost::shared_ptr<impl_type> > plugin_list_type;
		plugin_list_type plugins;
		boost::shared_ptr<impl_type> get(unsigned int id) {
			typename plugin_list_type::iterator it = plugins.find(id);
			if (it != plugins.end())
				return it->second;
			boost::shared_ptr<impl_type> impl = boost::shared_ptr<impl_type>(new impl_type());
			plugins[id] = impl;
			return impl; 
		}
		void erase(unsigned int id) {
			plugins.erase(id);
		}
	};

	struct helpers {
		static void wrap_string(std::string &string, char** buffer, unsigned int *buffer_len) {
			// TODO: Make this global to allow remote deletion!!!
			unsigned int buf_len = string.size();
			*buffer = new char[buf_len + 10];
			memcpy(*buffer, string.c_str(), buf_len+1);
			(*buffer)[buf_len] = 0;
			(*buffer)[buf_len+1] = 0;
			*buffer_len = buf_len;
		}
		int static wrap_string(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode ) {
			// @todo deprecate this
			if (str.length() >= bufLen) {
				std::wstring sstr = str.substr(0, bufLen-2);
				NSC_DEBUG_MSG_STD(_T("String (") + strEx::itos(str.length()) + _T(") to long to fit inside buffer(") + strEx::itos(bufLen) + _T(") : ") + sstr);
				return NSCAPI::isInvalidBufferLen;
			}
			wcsncpy(buffer, str.c_str(), bufLen);
			return defaultReturnCode;
		}

	};
	template<class impl_class>
	struct basic_wrapper_static {

		static int NSModuleHelperInit(nscapi::core_api::lpNSAPILoader f) {
			try { 
				return nscapi::plugin_singleton->get_core()->load_endpoints(f)?NSCAPI::isSuccess:NSCAPI::hasFailed;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: wrapModuleHelperInit")); 
				return NSCAPI::hasFailed; 
			} 
		}
		static int NSLoadModule() { 
			return NSCAPI::hasFailed; 
		} 
		static int NSGetModuleName(wchar_t* buf, int buflen) { 
			try {
				return helpers::wrap_string(buf, buflen, impl_class::getModuleName(), NSCAPI::isSuccess);
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSGetModuleName")); 
			} 
			return NSCAPI::hasFailed; 
		} 
		static int NSGetModuleDescription(wchar_t* buf, int buflen) { 
			try { 
				return helpers::wrap_string(buf, buflen, impl_class::getModuleDescription(), NSCAPI::isSuccess);
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSGetModuleDescription")); 
			} 
			return NSCAPI::hasFailed; 
		} 
		static int NSGetModuleVersion(int *major, int *minor, int *revision) { 
			try { 
				nscapi::plugin_wrapper::module_version version = impl_class::getModuleVersion();
				*major = version.major;
				*minor = version.minor;
				*revision = version.revision;
				return NSCAPI::isSuccess;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSGetModuleVersion")); 
			} 
			return NSCAPI::hasFailed; 
		}
		static void NSDeleteBuffer(char** buffer) { 
			try {
				delete [] *buffer;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSDeleteBuffer")); 
			} 
		}
	};

	template<class impl_class>
	struct basic_wrapper {
		boost::shared_ptr<impl_class> instance;
		basic_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}
		int NSLoadModuleEx(unsigned int id, wchar_t* alias, int mode) { 
			try { 
				instance->set_id(id);
				if (instance->loadModuleEx(alias, mode))
					return NSCAPI::isSuccess;
			} catch (...) {
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSLoadModuleEx")); 
			} 
			return NSCAPI::hasFailed;
		} 
		int NSUnloadModule() { 
			try { 
				if (instance->unloadModule())
					return NSCAPI::isSuccess;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSUnloadModule")); 
			} 
			return NSCAPI::hasFailed;
		}
	};
	template<class impl_class>
	struct message_wrapper {
		boost::shared_ptr<impl_class> instance;
		message_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}
		void NSHandleMessage(const char* request_buffer, unsigned int request_buffer_len) { 
			try { 
				instance->handleMessageRAW(std::string(request_buffer, request_buffer_len));
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSHandleMessage")); 
			} 
		} 
		NSCAPI::boolReturn NSHasMessageHandler() { 
			try {
				if (instance->hasMessageHandler())
					return NSCAPI::istrue;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSHasMessageHandler")); 
			} 
			return NSCAPI::isfalse; 
		}
	};
	template<class impl_class>
	struct command_wrapper {
		boost::shared_ptr<impl_class> instance;
		command_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		NSCAPI::nagiosReturn NSHandleCommand(const wchar_t* command, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len) { 
			try { 
				std::string request(request_buffer, request_buffer_len), reply;
				NSCAPI::nagiosReturn retCode = instance->handleRAWCommand(command, request, reply);
				helpers::wrap_string(reply, reply_buffer, reply_buffer_len);
				if (!nscapi::plugin_helper::isMyNagiosReturn(retCode)) {
					NSC_LOG_ERROR(_T("A module returned an invalid return code"));
				}
				return retCode;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSHandleCommand")); 
			} 
			return NSCAPI::returnIgnored; 
		} 
		NSCAPI::boolReturn NSHasCommandHandler() { 
			try { 
				if (instance->hasCommandHandler())
					return NSCAPI::istrue;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSHasCommandHandler")); 
			} 
			return NSCAPI::isfalse; 
		}
	};

	template<class impl_class>
	struct routing_wrapper {
		boost::shared_ptr<impl_class> instance;
		routing_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		NSCAPI::nagiosReturn NSRouteMessage(const wchar_t* channel, const wchar_t* command, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len) { 
			try { 
				std::string request(request_buffer, request_buffer_len), reply;
				NSCAPI::nagiosReturn retCode = instance->RAWRouteMessage(channel, command, request, reply); 
				helpers::wrap_string(reply, reply_buffer, reply_buffer_len);
				return retCode;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSRouteMessage")); 
			} 
			return NSCAPI::returnIgnored; 
		} 
		NSCAPI::boolReturn NSHasRoutingHandler() { 
			try { 
				if (instance->hasRoutingHandler())
					return NSCAPI::istrue;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSHasRoutingHandler")); 
			} 
			return NSCAPI::isfalse; 
		}
	};

	template<class impl_class>
	struct submission_wrapper {
		boost::shared_ptr<impl_class> instance;
		submission_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		NSCAPI::nagiosReturn NSHandleNotification(const wchar_t* channel, const char* buffer, unsigned int buffer_len, char** response_buffer, unsigned int *response_buffer_len) {
			try { 
				std::string request(buffer, buffer_len), reply;
				NSCAPI::nagiosReturn retCode = instance->handleRAWNotification(channel, request, reply); 
				helpers::wrap_string(reply, response_buffer, response_buffer_len);
				//return helpers::wrap_string(reply, response_buffer, response_buffer_len, retCode);
				return retCode;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSHandleNotification")); 
			} 
			return NSCAPI::returnIgnored; 
		} 
		NSCAPI::boolReturn NSHasNotificationHandler() { 
			try { 
				if (instance->hasNotificationHandler())
					return NSCAPI::istrue;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSHasNotificationHandler")); 
			} 
			return NSCAPI::isfalse; 
		}
	};

	template<class impl_class>
	struct cliexec_wrapper {
		boost::shared_ptr<impl_class> instance;
		cliexec_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		int NSCommandLineExec(wchar_t *command, char *request_buffer, unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
			try { 
				std::string request(request_buffer, request_buffer_len), reply;
				NSCAPI::nagiosReturn retCode = instance->commandRAWLineExec(command, request, reply); 
				helpers::wrap_string(reply, response_buffer, response_buffer_len);
				return retCode;
			} catch (...) { 
				NSC_LOG_CRITICAL(_T("Unknown exception in: NSCommandLineExec")); 
			} 
			return NSCAPI::hasFailed; 
		} 
	};
};
