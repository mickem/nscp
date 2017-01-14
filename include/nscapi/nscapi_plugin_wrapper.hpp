/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <list>
#include <map>
#include <string.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <NSCAPI.h>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <utf8.hpp>

namespace nscapi {

	struct module_version {
		int major;
		int minor;
		int revision;
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
		void add_alias(const unsigned int existing_id, const unsigned int new_id) {
			boost::shared_ptr<impl_type> old = get(existing_id);
			plugins[new_id] = old;
		}
	};

	struct helpers {


		static void wrap_string(const std::string &string, char** buffer, unsigned int *buffer_len) {
			// TODO: Make this global to allow remote deletion!!!
			size_t buf_len = string.size();
			*buffer = new char[buf_len + 10];
			memcpy(*buffer, string.c_str(), buf_len+1);
			(*buffer)[buf_len] = 0;
			(*buffer)[buf_len+1] = 0;
			*buffer_len = static_cast<unsigned int>(buf_len);
		}
		int static wrap_string(char *buffer, std::size_t bufLen, std::string str, int defaultReturnCode ) {
			// @todo deprecate this
			if (str.length() >= bufLen) {
				std::string sstr = str.substr(0, bufLen-2);
				return NSCAPI::isInvalidBufferLen;
			}
			strncpy(buffer, str.c_str(), bufLen);
			return defaultReturnCode;
		}
	};
	template<class impl_class>
	struct basic_wrapper_static {

		static int NSModuleHelperInit(nscapi::core_api::lpNSAPILoader f) {
			try { 
				return nscapi::plugin_singleton->get_core()->load_endpoints(f)?NSCAPI::api_return_codes::isSuccess:NSCAPI::api_return_codes::hasFailed;
			} catch (...) { 
				NSC_LOG_CRITICAL("Unknown exception in: wrapModuleHelperInit");
				return NSCAPI::api_return_codes::hasFailed;
			} 
		}
		static void set_alias(const char *default_alias, const char *alias) {
			nscapi::plugin_singleton->get_core()->set_alias(default_alias, alias);
		}
		static int NSLoadModule() { 
			return NSCAPI::api_return_codes::hasFailed; 
		} 
		static int NSGetModuleName(char* buf, int buflen) { 
			try {
				return helpers::wrap_string(buf, buflen, impl_class::getModuleName(), NSCAPI::api_return_codes::isSuccess);
			} catch (...) { 
				NSC_LOG_CRITICAL("Unknown exception in: NSGetModuleName");
			} 
			return NSCAPI::api_return_codes::hasFailed; 
		} 
		static int NSGetModuleDescription(char* buf, int buflen) { 
			try { 
				return helpers::wrap_string(buf, buflen, impl_class::getModuleDescription(), NSCAPI::api_return_codes::isSuccess);
			} catch (...) { 
				NSC_LOG_CRITICAL("Unknown exception in: NSGetModuleDescription");
			} 
			return NSCAPI::api_return_codes::hasFailed; 
		} 
		static int NSGetModuleVersion(int *major, int *minor, int *revision) { 
			try { 
				nscapi::module_version version = impl_class::getModuleVersion();
				*major = version.major;
				*minor = version.minor;
				*revision = version.revision;
				return NSCAPI::api_return_codes::isSuccess;
			} catch (...) { 
				NSC_LOG_CRITICAL("Unknown exception in: NSGetModuleVersion");
			} 
			return NSCAPI::api_return_codes::hasFailed; 
		}
		static void NSDeleteBuffer(char** buffer) { 
			try {
				delete [] *buffer;
			} catch (...) { 
				NSC_LOG_CRITICAL("Unknown exception in: NSDeleteBuffer");
			} 
		}
	};

	template<class impl_class>
	struct basic_wrapper {
		boost::shared_ptr<impl_class> instance;
		basic_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}
		int NSLoadModuleEx(unsigned int id, char* alias, int mode) { 
			try {
				return NSLoadModuleExNoExcept(id, alias, mode);
			} catch (const std::exception &e) {
				NSC_LOG_ERROR_EXR("NSLoadModuleEx", e);
			} catch (...) {
				NSC_LOG_CRITICAL("Unknown exception in: NSLoadModuleEx");
			} 
			return NSCAPI::api_return_codes::hasFailed;
		} 
		int NSLoadModuleExNoExcept(unsigned int id, char* alias, int mode) { 
			instance->set_id(id);
			return instance->loadModuleEx(alias, mode)?NSCAPI::api_return_codes::isSuccess:NSCAPI::api_return_codes::hasFailed;
		} 
		int NSUnloadModule() { 
			try { 
				if (instance && instance->unloadModule())
					return NSCAPI::api_return_codes::hasFailed;
			} catch (...) { 
				NSC_LOG_CRITICAL("Unknown exception in: NSUnloadModule");
			} 
			return NSCAPI::api_return_codes::hasFailed;
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
				NSC_LOG_CRITICAL("Unknown exception in: NSHandleMessage");
			} 
		} 
		NSCAPI::boolReturn NSHasMessageHandler() { 
			try {
				if (instance->hasMessageHandler())
					return NSCAPI::bool_return::istrue;
			} catch (...) { 
				NSC_LOG_CRITICAL("Unknown exception in: NSHasMessageHandler");
			} 
			return NSCAPI::bool_return::isfalse; 
		}
	};
	template<class impl_class>
	struct command_wrapper {
		boost::shared_ptr<impl_class> instance;
		command_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		NSCAPI::nagiosReturn NSHandleCommand(const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len) { 
			try { 
				std::string request(request_buffer, request_buffer_len), reply;
				NSCAPI::nagiosReturn retCode = instance->handleRAWCommand(request, reply);
				helpers::wrap_string(reply, reply_buffer, reply_buffer_len);
				if (!nscapi::plugin_helper::isMyNagiosReturn(retCode)) {
					NSC_LOG_ERROR("A module returned an invalid return code");
				}
				return retCode;
			} catch (const std::exception &e) { 
				NSC_LOG_ERROR_EXR("NSHandleCommand", e);
				return NSCAPI::cmd_return_codes::hasFailed;
			} catch (...) { 
				NSC_LOG_ERROR_EX("NSHandleCommand");
				return NSCAPI::cmd_return_codes::hasFailed;
			} 
			return NSCAPI::cmd_return_codes::returnIgnored; 
		} 
		NSCAPI::boolReturn NSHasCommandHandler() { 
			try { 
				if (instance->hasCommandHandler())
					return NSCAPI::bool_return::istrue;
			} catch (...) { 
				NSC_LOG_ERROR_EX("NSHasCommandHandler");
			} 
			return NSCAPI::bool_return::isfalse; 
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
				NSC_LOG_ERROR_EX("NSRouteMessage");
			} 
			return NSCAPI::cmd_return_codes::returnIgnored; 
		} 
		NSCAPI::boolReturn NSHasRoutingHandler() { 
			try { 
				if (instance->hasRoutingHandler())
					return NSCAPI::bool_return::istrue;
			} catch (...) { 
				NSC_LOG_ERROR_EX("NSHasRoutingHandler");
			} 
			return NSCAPI::bool_return::isfalse; 
		}
	};

	template<class impl_class>
	struct submission_wrapper {
		boost::shared_ptr<impl_class> instance;
		submission_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		NSCAPI::nagiosReturn NSHandleNotification(const char* channel, const char* buffer, unsigned int buffer_len, char** response_buffer, unsigned int *response_buffer_len) {
			try { 
				std::string request(buffer, buffer_len), reply;
				NSCAPI::nagiosReturn retCode = instance->handleRAWNotification(channel, request, reply); 
				helpers::wrap_string(reply, response_buffer, response_buffer_len);
				//return helpers::wrap_string(reply, response_buffer, response_buffer_len, retCode);
				return retCode;
			} catch (const std::exception &e) { 
				NSC_LOG_ERROR_EXR("NSHandleNotification", e);
			} catch (...) { 
				NSC_LOG_ERROR_EX("NSHandleNotification");
			} 
			return NSCAPI::cmd_return_codes::hasFailed; 
		} 
		NSCAPI::boolReturn NSHasNotificationHandler() { 
			try { 
				if (instance->hasNotificationHandler())
					return NSCAPI::bool_return::istrue;
			} catch (...) { 
				NSC_LOG_ERROR_EX("NSHasNotificationHandler");
			} 
			return NSCAPI::bool_return::isfalse; 
		}
	};

	template<class impl_class>
	struct cliexec_wrapper {
		boost::shared_ptr<impl_class> instance;
		cliexec_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		int NSCommandLineExec(const int target_mode, char *request_buffer, unsigned int request_buffer_len, char **response_buffer, unsigned int *response_buffer_len) {
			try { 
				std::string request(request_buffer, request_buffer_len), reply;
				NSCAPI::nagiosReturn retCode = instance->commandRAWLineExec(target_mode, request, reply);
				helpers::wrap_string(reply, response_buffer, response_buffer_len);
				return retCode;
			} catch (const std::exception &e) { 
				NSC_LOG_ERROR_EXR("NSCommandLineExec", e);
			} catch (...) { 
				NSC_LOG_ERROR_EX("NSCommandLineExec");
			} 
			return NSCAPI::cmd_return_codes::hasFailed; 
		} 
	};

	template<class impl_class>
	struct metrics_wrapper {
		boost::shared_ptr<impl_class> instance;
		metrics_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		int NSFetchMetrics(char **response_buffer, unsigned int *response_buffer_len) {
			try {
				std::string reply;
				NSCAPI::nagiosReturn retCode = instance->fetchMetrics(reply);
				helpers::wrap_string(reply, response_buffer, response_buffer_len);
				return retCode;
			}
			catch (const std::exception &e) {
				NSC_LOG_ERROR_EXR("NSFetchMetrics", e);
			}
			catch (...) {
				NSC_LOG_ERROR_EX("NSFetchMetrics");
			}
			return NSCAPI::api_return_codes::hasFailed;
		}
		int NSSubmitMetrics(const char *buffer, const unsigned int buffer_len) {
			try {
				std::string reply(buffer, buffer_len);
				return instance->submitMetrics(reply);
			} catch (const std::exception &e) {
				NSC_LOG_ERROR_EXR("NSFetchMetrics", e);
			} catch (...) {
				NSC_LOG_ERROR_EX("NSFetchMetrics");
			}
			return NSCAPI::api_return_codes::hasFailed;
		}
	};

	template<class impl_class>
	struct event_wrapper {
		boost::shared_ptr<impl_class> instance;
		event_wrapper(boost::shared_ptr<impl_class> instance) : instance(instance) {}

		int NSOnEvent(const char *buffer, const unsigned int buffer_len) {
			try {
				std::string message(buffer, buffer_len);
				return instance->onRAWEvent(message);
			} catch (const std::exception &e) {
				NSC_LOG_ERROR_EXR("NSOnEvent", e);
			} catch (...) {
				NSC_LOG_ERROR_EX("NSOnEvent");
			}
			return NSCAPI::api_return_codes::hasFailed;
		}
	};

}
