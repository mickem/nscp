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
//#include <iostream>

#include <NSCAPI.h>
//#include <charEx.h>
//#include <arrayBuffer.h>
//#include <types.hpp>

//#include <unicode_char.hpp>
//#include <strEx.h>

namespace nscapi {
	class core_wrapper {
	private:
		nscapi::core_api::lpNSAPIGetBasePath fNSAPIGetBasePath;
		nscapi::core_api::lpNSAPIGetApplicationName fNSAPIGetApplicationName;
		nscapi::core_api::lpNSAPIGetApplicationVersionStr fNSAPIGetApplicationVersionStr;
		nscapi::core_api::lpNSAPIGetSettingsSection fNSAPIGetSettingsSection;
		nscapi::core_api::lpNSAPIReleaseSettingsSectionBuffer fNSAPIReleaseSettingsSectionBuffer;
		nscapi::core_api::lpNSAPIGetSettingsString fNSAPIGetSettingsString;
		nscapi::core_api::lpNSAPIExpandPath fNSAPIExpandPath;
		nscapi::core_api::lpNSAPIGetSettingsInt fNSAPIGetSettingsInt;
		nscapi::core_api::lpNSAPIGetSettingsBool fNSAPIGetSettingsBool;
		nscapi::core_api::lpNSAPIMessage fNSAPIMessage;
		nscapi::core_api::lpNSAPIStopServer fNSAPIStopServer;
		nscapi::core_api::lpNSAPIExit fNSAPIExit;
		nscapi::core_api::lpNSAPIInject fNSAPIInject;
		nscapi::core_api::lpNSAPIExecCommand fNSAPIExecCommand;
		nscapi::core_api::lpNSAPIDestroyBuffer fNSAPIDestroyBuffer;
		nscapi::core_api::lpNSAPINotify fNSAPINotify;
		nscapi::core_api::lpNSAPICheckLogMessages fNSAPICheckLogMessages;
		nscapi::core_api::lpNSAPIEncrypt fNSAPIEncrypt;
		nscapi::core_api::lpNSAPIDecrypt fNSAPIDecrypt;
		nscapi::core_api::lpNSAPISetSettingsString fNSAPISetSettingsString;
		nscapi::core_api::lpNSAPISetSettingsInt fNSAPISetSettingsInt;
		nscapi::core_api::lpNSAPIWriteSettings fNSAPIWriteSettings;
		nscapi::core_api::lpNSAPIReadSettings fNSAPIReadSettings;
		nscapi::core_api::lpNSAPIRehash fNSAPIRehash;
		nscapi::core_api::lpNSAPIDescribeCommand fNSAPIDescribeCommand;
		nscapi::core_api::lpNSAPIGetAllCommandNames fNSAPIGetAllCommandNames;
		nscapi::core_api::lpNSAPIReleaseAllCommandNamessBuffer fNSAPIReleaseAllCommandNamessBuffer;
		nscapi::core_api::lpNSAPIRegisterCommand fNSAPIRegisterCommand;
		nscapi::core_api::lpNSAPISettingsRegKey fNSAPISettingsRegKey;
		nscapi::core_api::lpNSAPISettingsRegPath fNSAPISettingsRegPath;
		nscapi::core_api::lpNSAPIGetPluginList fNSAPIGetPluginList;
		nscapi::core_api::lpNSAPIReleasePluginList fNSAPIReleasePluginList;
		nscapi::core_api::lpNSAPISettingsSave fNSAPISettingsSave;
		nscapi::core_api::lpNSAPIRegisterSubmissionListener fNSAPIRegisterSubmissionListener;
		nscapi::core_api::lpNSAPIRegisterRoutingListener fNSAPIRegisterRoutingListener;

	public:

		struct plugin_info_type {
			std::wstring dll;
			std::wstring name;
			std::wstring version;
			std::wstring description;
		};
		typedef std::list<plugin_info_type> plugin_info_list;

		core_wrapper() 
			: fNSAPIGetBasePath(NULL)
			, fNSAPIGetApplicationName(NULL)
			, fNSAPIGetApplicationVersionStr(NULL)
			, fNSAPIGetSettingsSection(NULL)
			, fNSAPIReleaseSettingsSectionBuffer(NULL)
			, fNSAPIGetSettingsString(NULL)
			, fNSAPIGetSettingsInt(NULL)
			, fNSAPIGetSettingsBool(NULL)
			, fNSAPIMessage(NULL)
			, fNSAPIStopServer(NULL)
			, fNSAPIExit(NULL)
			, fNSAPIInject(NULL)
			, fNSAPIDestroyBuffer(NULL)
			, fNSAPINotify(NULL)
			, fNSAPICheckLogMessages(NULL)
			, fNSAPIEncrypt(NULL)
			, fNSAPIDecrypt(NULL)
			, fNSAPISetSettingsString(NULL)
			, fNSAPISetSettingsInt(NULL)
			, fNSAPIWriteSettings(NULL)
			, fNSAPIReadSettings(NULL)
			, fNSAPIRehash(NULL)
			, fNSAPIDescribeCommand(NULL)
			, fNSAPIGetAllCommandNames(NULL)
			, fNSAPIReleaseAllCommandNamessBuffer(NULL)
			, fNSAPIRegisterCommand(NULL)
			, fNSAPISettingsRegKey(NULL)
			, fNSAPISettingsRegPath(NULL)
			, fNSAPIGetPluginList(NULL)
			, fNSAPIReleasePluginList(NULL)
			, fNSAPISettingsSave(NULL)
			, fNSAPIExpandPath(NULL)
		{}

		// Helper functions for calling into the core
		std::wstring getApplicationName(void);
		std::wstring getApplicationVersionString(void);
		std::list<std::wstring> getSettingsSection(std::wstring section);
		std::wstring getSettingsString(std::wstring section, std::wstring key, std::wstring defaultValue);
		std::wstring expand_path(std::wstring value);
		int getSettingsInt(std::wstring section, std::wstring key, int defaultValue);
		bool getSettingsBool(std::wstring section, std::wstring key, bool defaultValue);
		void settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced);
		void settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced);
		void settings_save();

		void Message(int msgType, std::string file, int line, std::wstring message);
		void DestroyBuffer(char**buffer);
		NSCAPI::nagiosReturn query(const wchar_t* command, const char *request, const unsigned int request_len, char **response, unsigned int *response_len);
		NSCAPI::nagiosReturn query(const std::wstring & command, const std::string & request, std::string & result);
		NSCAPI::nagiosReturn simple_query(const std::wstring command, const std::list<std::wstring> & argument, std::wstring & message, std::wstring & perf);
		NSCAPI::nagiosReturn simple_query(const std::wstring command, const std::list<std::wstring> & argument, std::string & result);
		NSCAPI::nagiosReturn simple_query_from_nrpe(const std::wstring command, const std::wstring & buffer, std::wstring & message, std::wstring & perf);

		NSCAPI::nagiosReturn exec_command(const wchar_t* command, const char *request, const unsigned int request_len, char **response, unsigned int *response_len);
		NSCAPI::nagiosReturn exec_command(const std::wstring command, std::string request, std::string & result);
		NSCAPI::nagiosReturn exec_simple_command(const std::wstring command, const std::list<std::wstring> &argument, std::list<std::wstring> & result);

		bool submit_simple_message(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn code, std::wstring & message, std::wstring & perf, std::wstring & response);
		NSCAPI::errorReturn submit_message(const wchar_t* channel, const char *request, const unsigned int request_len, char **response, unsigned int *response_len);
		NSCAPI::errorReturn submit_message(std::wstring channel, std::string request, std::string &response);
		void StopService(void);
		void Exit(void);
		std::wstring getBasePath();
		bool logDebug();
		bool checkLogMessages(int type);
		std::wstring Encrypt(std::wstring str, unsigned int algorithm = NSCAPI::encryption_xor);
		std::wstring Decrypt(std::wstring str, unsigned int algorithm = NSCAPI::encryption_xor);
		NSCAPI::errorReturn SetSettingsString(std::wstring section, std::wstring key, std::wstring value);
		NSCAPI::errorReturn SetSettingsInt(std::wstring section, std::wstring key, int value);
		NSCAPI::errorReturn WriteSettings(int type);
		NSCAPI::errorReturn ReadSettings(int type);
		NSCAPI::errorReturn Rehash(int flag);
		plugin_info_list getPluginList();

		std::list<std::wstring> getAllCommandNames();
		std::wstring describeCommand(std::wstring command);
		void registerCommand(unsigned int id, std::wstring command, std::wstring description);
		void registerSubmissionListener(unsigned int id, std::wstring channel);
		void registerRoutingListener(unsigned int id, std::wstring channel);

		unsigned int getBufferLength();
		bool load_endpoints(nscapi::core_api::lpNSAPILoader f);

	};
};
