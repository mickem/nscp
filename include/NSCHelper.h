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

#ifdef WIN32
//#include <windows.h>
#endif


namespace NSCHelper
{
#ifdef DEBUG
	NSCAPI::nagiosReturn wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, NSCAPI::nagiosReturn defaultReturnCode);
	NSCAPI::errorReturn wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, NSCAPI::errorReturn defaultReturnCode);
#else
	int wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode);
#endif
	std::wstring translateMessageType(NSCAPI::messageTypes msgType);
	std::wstring translateReturn(NSCAPI::nagiosReturn returnCode);
	NSCAPI::nagiosReturn translateReturn(std::wstring str);
	NSCAPI::nagiosReturn maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b);

	inline bool isNagiosReturnCode(NSCAPI::nagiosReturn code) {
		if ( (code == NSCAPI::returnOK) || (code == NSCAPI::returnWARN) || (code == NSCAPI::returnCRIT) || (code == NSCAPI::returnUNKNOWN) )
			return true;
		return false;
	}
	inline bool isMyNagiosReturn(NSCAPI::nagiosReturn code) {
		return code == NSCAPI::returnCRIT || code == NSCAPI::returnOK || code == NSCAPI::returnWARN || code == NSCAPI::returnUNKNOWN 
			|| code == NSCAPI::returnInvalidBufferLen || code == NSCAPI::returnIgnored;
	}

#ifdef DEBUG
	inline NSCAPI::nagiosReturn int2nagios(int code) {
		if (code == 0)
			return NSCAPI::returnOK;
		if (code == 1)
			return NSCAPI::returnWARN;
		if (code == 2)
			return NSCAPI::returnCRIT;
		if (code == 4)
			return NSCAPI::returnUNKNOWN;
		throw "@fixme bad code";
	}
	inline int nagios2int(NSCAPI::nagiosReturn code) {
		if (code == NSCAPI::returnOK)
			return 0;
		if (code == NSCAPI::returnWARN)
			return 1;
		if (code == NSCAPI::returnCRIT)
			return 2;
		if (code == NSCAPI::returnUNKNOWN)
			return 4;
		throw "@fixme bad code";
	}
#else
	inline NSCAPI::nagiosReturn int2nagios(int code) {
		return code;
	}
	inline int nagios2int(NSCAPI::nagiosReturn code) {
		return code;
	}
#endif
	inline void escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode) {
		currentReturnCode = NSCAPI::returnCRIT;
	}
	inline void escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode) {
		if (currentReturnCode != NSCAPI::returnCRIT)
			currentReturnCode = NSCAPI::returnWARN;
	}

	namespace report {
		unsigned int parse(std::wstring str);
		bool matches(unsigned int report, NSCAPI::nagiosReturn code);
		std::wstring to_string(unsigned int report);
	}
	namespace logging {
		unsigned int parse(std::wstring str);
		bool matches(unsigned int report, NSCAPI::nagiosReturn code);
		std::wstring to_string(unsigned int report);
	}
};

namespace NSCModuleHelper
{
	class NSCMHExcpetion {
	public:
		std::wstring msg_;
		NSCMHExcpetion(std::wstring msg) : msg_(msg) {}
	};
	struct plugin_info_type {
		std::wstring dll;
		std::wstring name;
		std::wstring version;
		std::wstring description;
	};
	typedef std::list<plugin_info_type> plugin_info_list;
	// Types for the Callbacks into the main program
	
	typedef NSCAPI::errorReturn (*lpNSAPIGetBasePath)(wchar_t*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationName)(wchar_t*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationVersionStr)(wchar_t*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsString)(const wchar_t*,const wchar_t*,const wchar_t*,wchar_t*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsInt)(const wchar_t*, const wchar_t*, int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetSettingsSection)(const wchar_t*, arrayBuffer::arrayBuffer*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIReleaseSettingsSectionBuffer)(arrayBuffer::arrayBuffer*, unsigned int *);
	typedef void (*lpNSAPIMessage)(int, const wchar_t*, const int, const wchar_t*);
	typedef NSCAPI::errorReturn (*lpNSAPIStopServer)(void);
	typedef NSCAPI::errorReturn (*lpNSAPIExit)(void);
	typedef NSCAPI::nagiosReturn (*lpNSAPIInject)(const wchar_t*, const char *, const unsigned int, char **, unsigned int *);
	typedef void (*lpNSAPIDestroyBuffer)(char**);

	typedef NSCAPI::errorReturn (*lpNSAPINotify)(const wchar_t*, const wchar_t*, NSCAPI::nagiosReturn, const char*, unsigned int);

	typedef NSCAPI::boolReturn (*lpNSAPICheckLogMessages)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIEncrypt)(unsigned int, const wchar_t*, unsigned int, wchar_t*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIDecrypt)(unsigned int, const wchar_t*, unsigned int, wchar_t*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPISetSettingsString)(const wchar_t*, const wchar_t*, const wchar_t*);
	typedef NSCAPI::errorReturn (*lpNSAPISetSettingsInt)(const wchar_t*, const wchar_t*, int);
	typedef NSCAPI::errorReturn (*lpNSAPIWriteSettings)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIReadSettings)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIRehash)(int);
	typedef NSCAPI::errorReturn (*lpNSAPIDescribeCommand)(const wchar_t*,wchar_t*,unsigned int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetAllCommandNames)(arrayBuffer::arrayBuffer*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIReleaseAllCommandNamessBuffer)(arrayBuffer::arrayBuffer*, unsigned int *);
	typedef NSCAPI::errorReturn (*lpNSAPIRegisterCommand)(unsigned int, const wchar_t*,const wchar_t*);
	typedef NSCAPI::errorReturn (*lpNSAPISettingsRegKey)(const wchar_t*, const wchar_t*, int, const wchar_t*, const wchar_t*, const wchar_t*, int);
	typedef NSCAPI::errorReturn (*lpNSAPISettingsRegPath)(const wchar_t*, const wchar_t*, const wchar_t*, int);
	typedef NSCAPI::errorReturn (*lpNSAPIGetPluginList)(int *len, NSCAPI::plugin_info *list[]);
	typedef NSCAPI::errorReturn (*lpNSAPIReleasePluginList)(int len, NSCAPI::plugin_info *list[]);
	typedef NSCAPI::errorReturn (*lpNSAPISettingsSave)(void);


	// Helper functions for calling into the core
	std::wstring getApplicationName(void);
	std::wstring getApplicationVersionString(void);
	std::list<std::wstring> getSettingsSection(std::wstring section);
	std::wstring getSettingsString(std::wstring section, std::wstring key, std::wstring defaultValue);
	int getSettingsInt(std::wstring section, std::wstring key, int defaultValue);
	void settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced);
	void settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced);
	void settings_save();

	void Message(int msgType, std::wstring file, int line, std::wstring message);
	NSCAPI::nagiosReturn InjectCommandRAW(const wchar_t* command, const char *request, const unsigned int request_len, char **response, unsigned int *response_len);
	void DestroyBuffer(char**buffer);
	NSCAPI::nagiosReturn InjectCommand(const std::wstring command, const std::list<std::wstring> argument, std::string & result);
	NSCAPI::nagiosReturn InjectSimpleCommand(const std::wstring command, const std::list<std::wstring> argument, std::wstring & message, std::wstring & perf);
	NSCAPI::errorReturn NotifyChannel(std::wstring channel, std::wstring command, NSCAPI::nagiosReturn code, std::string result);
	NSCAPI::nagiosReturn InjectSplitAndCommand(const wchar_t* command, wchar_t* buffer, wchar_t splitChar, std::wstring & message, std::wstring & perf);
	NSCAPI::nagiosReturn InjectSplitAndCommand(const std::wstring command, const std::wstring buffer, wchar_t splitChar, std::wstring & message, std::wstring & perf, bool escape = false);
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
	void registerCommand(std::wstring command, std::wstring description);
	unsigned int getBufferLength();


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
				//NSC_LOG_ERROR_STD(_T("Unsupported payload size: ") + to_wstring(request_message.payload_size()));
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
			//(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) = 0;
		//(const std::wstring command, const std::list<std::wstring> arguments) = 0;

	};
};


#define SETTINGS_MAKE_NAME(key) \
	std::wstring(setting_keys::key ## _PATH + _T(".") + setting_keys::key)

#define SETTINGS_GET_STRING(key) \
	NSCModuleHelper::getSettingsString(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)
#define SETTINGS_GET_INT(key) \
	NSCModuleHelper::getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)
#define SETTINGS_GET_BOOL(key) \
	NSCModuleHelper::getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT)

#define SETTINGS_GET_STRING_FALLBACK(key, fallback) \
	NSCModuleHelper::getSettingsString(setting_keys::key ## _PATH, setting_keys::key, NSCModuleHelper::getSettingsString(setting_keys::fallback ## _PATH, setting_keys::fallback, setting_keys::fallback ## _DEFAULT))
#define SETTINGS_GET_INT_FALLBACK(key, fallback) \
	NSCModuleHelper::getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, NSCModuleHelper::getSettingsInt(setting_keys::fallback ## _PATH, setting_keys::fallback, setting_keys::fallback ## _DEFAULT))
#define SETTINGS_GET_BOOL_FALLBACK(key, fallback) \
	NSCModuleHelper::getSettingsInt(setting_keys::key ## _PATH, setting_keys::key, NSCModuleHelper::getSettingsInt(setting_keys::fallback ## _PATH, setting_keys::fallback, setting_keys::fallback ## _DEFAULT))

#define SETTINGS_REG_KEY_S(key) \
	NSCModuleHelper::settings_register_key(setting_keys::key ## _PATH, setting_keys::key, NSCAPI::key_string, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT, setting_keys::key ## _ADVANCED);
#define SETTINGS_REG_KEY_I(key) \
	NSCModuleHelper::settings_register_key(setting_keys::key ## _PATH, setting_keys::key, NSCAPI::key_integer, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, boost::lexical_cast<std::wstring>(setting_keys::key ## _DEFAULT), setting_keys::key ## _ADVANCED);
#define SETTINGS_REG_KEY_B(key) \
	NSCModuleHelper::settings_register_key(setting_keys::key ## _PATH, setting_keys::key, NSCAPI::key_integer, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT==1?_T("1"):_T("0"), setting_keys::key ## _ADVANCED);
#define SETTINGS_REG_PATH(key) \
	NSCModuleHelper::settings_register_path(setting_keys::key ## _PATH, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _ADVANCED);
