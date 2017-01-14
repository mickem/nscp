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