/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

#include <NSCAPI.h>
#include <nscapi/dll_defines.hpp>

namespace nscapi {
	class plugin_helper {
	public:
		static NSCAPI_EXPORT bool isNagiosReturnCode(NSCAPI::nagiosReturn code);
		static NSCAPI_EXPORT bool isMyNagiosReturn(NSCAPI::nagiosReturn code);
		static NSCAPI_EXPORT NSCAPI::nagiosReturn int2nagios(int code);
		static NSCAPI_EXPORT int nagios2int(NSCAPI::nagiosReturn code);
		static NSCAPI_EXPORT void escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode);
		static NSCAPI_EXPORT void escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode);
		static NSCAPI_EXPORT int wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode);
		//		static NSCAPI_EXPORT std::wstring translateMessageType(NSCAPI::messageTypes msgType);
		static NSCAPI_EXPORT std::string translateReturn(NSCAPI::nagiosReturn returnCode);
		static NSCAPI_EXPORT NSCAPI::nagiosReturn translateReturn(std::string str);
		static NSCAPI_EXPORT NSCAPI::nagiosReturn maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b);
	};

	namespace report {
		NSCAPI_EXPORT unsigned int parse(std::string str);
		NSCAPI_EXPORT bool matches(unsigned int report, NSCAPI::nagiosReturn code);
		NSCAPI_EXPORT std::string to_string(unsigned int report);
	}
	namespace logging {
		NSCAPI_EXPORT NSCAPI::log_level::level parse(std::string str);
		NSCAPI_EXPORT bool matches(NSCAPI::log_level::level level, NSCAPI::nagiosReturn code);
		NSCAPI_EXPORT std::string to_string(NSCAPI::log_level::level level);
	}
}