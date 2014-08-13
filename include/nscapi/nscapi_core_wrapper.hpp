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

namespace nscapi {
	class core_wrapper {
	private:
		std::string alias;	// This is actually the wrong value if multiple modules are loaded!
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
		nscapi::core_api::lpNSAPIEncrypt fNSAPIEncrypt;
		nscapi::core_api::lpNSAPIDecrypt fNSAPIDecrypt;
		nscapi::core_api::lpNSAPISettingsQuery fNSAPISettingsQuery;
		nscapi::core_api::lpNSAPIExpandPath fNSAPIExpandPath;
		nscapi::core_api::lpNSAPIGetLoglevel fNSAPIGetLoglevel;
		nscapi::core_api::lpNSAPIRegistryQuery fNSAPIRegistryQuery;
		nscapi::core_api::lpNSCAPIJson2Protobuf fNSCAPIJson2Protobuf;
		nscapi::core_api::lpNSCAPIProtobuf2Json fNSCAPIProtobuf2Json;
		

	public:

		struct plugin_info_type {
			std::wstring dll;
			std::wstring name;
			std::wstring version;
			std::wstring description;
		};
		typedef std::list<plugin_info_type> plugin_info_list;

		core_wrapper() 
			: fNSAPIGetApplicationName(NULL)
			, fNSAPIGetApplicationVersionStr(NULL)
			, fNSAPIMessage(NULL)
			, fNSAPISimpleMessage(NULL)
			, fNSAPIInject(NULL)
			, fNSAPIExecCommand(NULL)
			, fNSAPIDestroyBuffer(NULL)
			, fNSAPINotify(NULL)
			, fNSAPIReload(NULL)
			, fNSAPICheckLogMessages(NULL)
			, fNSAPIEncrypt(NULL)
			, fNSAPIDecrypt(NULL)
			, fNSAPISettingsQuery(NULL)
			, fNSAPIExpandPath(NULL)
			, fNSAPIGetLoglevel(NULL)
			, fNSAPIRegistryQuery(NULL)
			, fNSCAPIJson2Protobuf(NULL)
			, fNSCAPIProtobuf2Json(NULL)
		{}

		std::string expand_path(std::string value);

		NSCAPI::errorReturn settings_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		bool settings_query(const std::string request, std::string &response) const;

		// Helper functions for calling into the core
		std::string getApplicationName(void);
		std::string getApplicationVersionString(void);

		void log(NSCAPI::nagiosReturn msgType, std::string file, int line, std::string message) const ;
		void log(std::string message) const ;
		bool should_log(NSCAPI::nagiosReturn msgType) const ;
		NSCAPI::log_level::level get_loglevel() const ;
		void DestroyBuffer(char**buffer) const;
		NSCAPI::nagiosReturn query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const ;
		NSCAPI::nagiosReturn query(const std::string & request, std::string & result) const ;

		NSCAPI::nagiosReturn exec_command(const char* target, const char *request, const unsigned int request_len, char **response, unsigned int *response_len);
		NSCAPI::nagiosReturn exec_command(const std::string target, std::string request, std::string & result);

		NSCAPI::errorReturn submit_message(const char* channel, const char *request, const unsigned int request_len, char **response, unsigned int *response_len);
		NSCAPI::errorReturn submit_message(std::string channel, std::string request, std::string &response);
		NSCAPI::errorReturn reload(std::string module) const;

		NSCAPI::nagiosReturn json_to_protobuf(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const ;
		bool json_to_protobuf(const std::string & request, std::string & result) const ;
		NSCAPI::nagiosReturn protobuf_to_json(const char *object, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const ;
		bool protobuf_to_json(const std::string &object, const std::string & request, std::string & result) const ;

		bool checkLogMessages(int type);
		std::wstring Encrypt(std::wstring str, unsigned int algorithm = NSCAPI::encryption_xor);
		std::wstring Decrypt(std::wstring str, unsigned int algorithm = NSCAPI::encryption_xor);

		NSCAPI::errorReturn registry_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
		NSCAPI::errorReturn registry_query(const std::string request, std::string &response) const;

		bool load_endpoints(nscapi::core_api::lpNSAPILoader f);
		void set_alias(const std::string default_alias, const std::string alias);
	};
}
