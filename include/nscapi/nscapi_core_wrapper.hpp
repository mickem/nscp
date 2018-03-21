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

#include <NSCAPI.h>
#include <nscapi/dll_defines.hpp>

#include <string>
#include <list>

namespace nscapi {
	class core_wrapper_impl;
	class NSCAPI_EXPORT core_wrapper {
	private:
		core_wrapper_impl* pimpl;
		nscapi::core_api::lpNSAPIGetApplicationName fNSAPIGetApplicationName;
		nscapi::core_api::lpNSAPIGetApplicationVersionStr fNSAPIGetApplicationVersionStr;
		nscapi::core_api::lpNSAPIMessage fNSAPIMessage;
		nscapi::core_api::lpNSAPISimpleMessage fNSAPISimpleMessage;
		nscapi::core_api::lpNSAPIInject fNSAPIInject;
		nscapi::core_api::lpNSAPIExecCommand fNSAPIExecCommand;
		nscapi::core_api::lpNSAPIDestroyBuffer fNSAPIDestroyBuffer;
		nscapi::core_api::lpNSAPINotify fNSAPINotify;
		nscapi::core_api::lpNSAPIReload fNSAPIReload;
		nscapi::core_api::lpNSAPICheckLogMessages fNSAPICheckLogMessages;
		nscapi::core_api::lpNSAPISettingsQuery fNSAPISettingsQuery;
		nscapi::core_api::lpNSAPIExpandPath fNSAPIExpandPath;
		nscapi::core_api::lpNSAPIGetLoglevel fNSAPIGetLoglevel;
		nscapi::core_api::lpNSAPIRegistryQuery fNSAPIRegistryQuery;
		nscapi::core_api::lpNSCAPIJson2Protobuf fNSCAPIJson2Protobuf;
		nscapi::core_api::lpNSCAPIProtobuf2Json fNSCAPIProtobuf2Json;
		nscapi::core_api::lpNSCAPIEmitEvent fNSCAPIEmitEvent;
		nscapi::core_api::lpNSAPIStorageQuery fNSAPIStorageQuery;

	public:

		core_wrapper();
		~core_wrapper();

		std::string expand_path(std::string value) const;

		NSCAPI::errorReturn settings_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool settings_query(const std::string request, std::string &response) const;

		// Helper functions for calling into the core
		std::string getApplicationName(void) const;
		std::string getApplicationVersionString(void) const;

		void log(NSCAPI::nagiosReturn msgType, std::string file, int line, std::string message) const;
		void log(std::string message) const;
		bool should_log(NSCAPI::nagiosReturn msgType) const;
		NSCAPI::log_level::level get_loglevel() const;
		void DestroyBuffer(char**buffer) const;
		NSCAPI::nagiosReturn query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool query(const std::string & request, std::string & result) const;

		NSCAPI::nagiosReturn exec_command(const char* target, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool exec_command(const std::string target, std::string request, std::string & result) const;

		NSCAPI::errorReturn submit_message(const char* channel, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		NSCAPI::errorReturn emit_event(const char *request, const unsigned int request_len) const;
		bool submit_message(std::string channel, std::string request, std::string &response) const;
		bool reload(std::string module) const;

		NSCAPI::nagiosReturn json_to_protobuf(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool json_to_protobuf(const std::string & request, std::string & result) const;
		NSCAPI::nagiosReturn protobuf_to_json(const char *object, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool protobuf_to_json(const std::string &object, const std::string & request, std::string & result) const;

		bool checkLogMessages(int type);

		NSCAPI::errorReturn registry_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool registry_query(const std::string request, std::string &response) const;

		NSCAPI::errorReturn storage_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool storage_query(const std::string request, std::string &response) const;

		bool load_endpoints(nscapi::core_api::lpNSAPILoader f);
		void set_alias(const std::string default_alias, const std::string alias);
	};
}