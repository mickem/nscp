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

namespace NSCAPI {

	const int normalStart = 0;
	const int dontStart = 1;
	const int reloadStart = 2;
	const int returnOK = 0;
	const int returnWARN = 1;
	const int returnCRIT = 2;
	const int returnUNKNOWN = 3;
	const int returnInvalidBufferLen = -2;
	const int returnIgnored = -1;
	const int istrue = 1;
	const int isfalse = 0;
	const int isSuccess = 1;
	const int hasFailed = 0;
	const int isInvalidBufferLen = -2;
	const int key_string = 100;
	const int key_integer = 200;
	const int key_bool = 300;


	const int target_module = 1;
	const int target_any = 0;

	const int message_processed	= 0x01;
	const int message_routed	= 0x02;
	const int message_ignored	= 0x04;
	const int message_digested	= 0x08;
	const int message_modified	= 0x10;

	typedef int nagiosReturn;
	typedef int boolReturn;
	typedef int errorReturn;
	typedef int settings_type;
	typedef int moduleLoadMode;

	const unsigned int encryption_xor = 1;

	// Settings types
	const int settings_default = 0;
	const int settings_registry = 1;
	const int settings_inifile = 2;

	// Various message Types
	namespace log_level {
		typedef int level;
		const int off = 0;		// Used to disable logging
		const int critical = 1;	// Critical error
		const int error = 10;	// Error
		const int warning = 50;	// Warning			<<< Default for command line interface
		//const int log = 100;	// Log message		<<< Default for service
		const int info = 150;	// information
		const int debug = 500;	// Debug messages	<<< Default for test
		const int trace = 1000;	// Trace messages
		const int ignore = 5000;// Ignored level (used for controlling states)
		const int unknown = 9999;// Ignored level (used for controlling states)
	}

	typedef log_level::level messageTypes;		// Message type
}

namespace nscapi {
	class nscapi_exception : public std::exception {
	public:
		std::string msg;
		nscapi_exception(std::string msg) : msg(msg) {}

		~nscapi_exception() throw() {}
		const char* what() const throw() {
			return msg.c_str();
		}
	};

	namespace core_api {
		typedef void* (*lpNSAPILoader)(const char*);

		typedef void (*lpNSAPIDestroyBuffer)(char**);

		typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationName)(char*,unsigned int);
		typedef NSCAPI::errorReturn (*lpNSAPIGetApplicationVersionStr)(char*,unsigned int);
		typedef NSCAPI::errorReturn (*lpNSAPIExpandPath)(const char*,char*,unsigned int);
		typedef NSCAPI::errorReturn (*lpNSAPIReload)(const char* module);
		typedef NSCAPI::log_level::level (*lpNSAPIGetLoglevel)();
		typedef NSCAPI::errorReturn (*lpNSAPISettingsQuery)(const char *, const unsigned int, char **, unsigned int *);
		typedef NSCAPI::errorReturn (*lpNSAPIRegistryQuery)(const char *, const unsigned int, char **, unsigned int *);
		typedef NSCAPI::errorReturn (*lpNSCAPIJson2Protobuf)(const char *, const unsigned int, char **, unsigned int *);
		typedef NSCAPI::errorReturn (*lpNSCAPIProtobuf2Json)(const char *, const char *, const unsigned int, char **, unsigned int *);

		typedef void (*lpNSAPIMessage)(const char*, unsigned int);
		typedef void (*lpNSAPISimpleMessage)(const char*, int, const char*, int, const char*);

		typedef NSCAPI::nagiosReturn (*lpNSAPIInject)(const char *, const unsigned int, char **, unsigned int *);
		typedef NSCAPI::nagiosReturn (*lpNSAPIExecCommand)(const char* target, const char *request, const unsigned int request_len, char ** response, unsigned int * response_len);
		typedef NSCAPI::errorReturn (*lpNSAPINotify)(const char* channel, const char* buffer, unsigned int buffer_len, char ** result_buffer, unsigned int *result_buffer_len);

		// TODO: investigate
		typedef NSCAPI::boolReturn (*lpNSAPICheckLogMessages)(int);

		// TODO: convert and re-add these
		typedef NSCAPI::errorReturn (*lpNSAPIEncrypt)(unsigned int, const wchar_t*, unsigned int, wchar_t*, unsigned int *);
		typedef NSCAPI::errorReturn (*lpNSAPIDecrypt)(unsigned int, const wchar_t*, unsigned int, wchar_t*, unsigned int *);
	}

	namespace plugin_api {
		typedef NSCAPI::errorReturn (*lpGetName)(char*,unsigned int);
		typedef NSCAPI::errorReturn (*lpGetDescription)(char*,unsigned int);
		typedef NSCAPI::errorReturn (*lpModuleHelperInit)(unsigned int, ::nscapi::core_api::lpNSAPILoader f);
		typedef NSCAPI::errorReturn (*lpGetVersion)(int* major, int* minor, int* revision);
		typedef NSCAPI::errorReturn (*lpDeleteBuffer)(char** buffer);

		typedef NSCAPI::errorReturn (*lpLoadModule)(unsigned int plugin_id, const char* alias, int mode);
		typedef NSCAPI::errorReturn (*lpUnLoadModule)(unsigned int plugin_id);

		typedef NSCAPI::errorReturn (*lpHasCommandHandler)(unsigned int plugin_id);
		typedef NSCAPI::errorReturn (*lpHandleCommand)(unsigned int plugin_id, const char* in_buffer, const unsigned int in_buffer_len, char** out_buffer, unsigned int* out_buffer_len);

		typedef NSCAPI::errorReturn (*lpHasMessageHandler)(unsigned int plugin_id);
		typedef NSCAPI::errorReturn (*lpHandleMessage)(unsigned int plugin_id, const char* buffer, const unsigned int buffer_len);

		typedef NSCAPI::errorReturn (*lpHasNotificationHandler)(unsigned int plugin_id);
		typedef NSCAPI::errorReturn (*lpHandleNotification)(unsigned int plugin_id, const char *channel, const char* buffer, unsigned int buffer_len, char **result_buffer, unsigned int *result_buffer_len);

		typedef NSCAPI::errorReturn (*lpHasRoutingHandler)(unsigned int plugin_id);
		typedef NSCAPI::errorReturn (*lpRouteMessage)(unsigned int plugin_id, const char *channel, const char* buffer, unsigned int buffer_len, char **new_channel_buffer, char **new_buffer, unsigned int *new_buffer_len);

		typedef NSCAPI::errorReturn (*lpCommandLineExec)(unsigned int plugin_id, const int targetFlag, const char* in_buffer ,const unsigned int in_buffer_len, char** out_buffer, unsigned int* out_buffer_len);

		typedef NSCAPI::errorReturn (*lpHandleSchedule)(unsigned int plugin_id, const char* in_buffer, const unsigned int in_buffer_len);

		typedef NSCAPI::errorReturn(*lpFetchMetrics)(unsigned int plugin_id, char **return_buffer, unsigned int *return_buffer_len);
		typedef NSCAPI::errorReturn(*lpSubmitMetrics)(unsigned int plugin_id, const char *buffer, const unsigned int buffer_len);


	}
}
