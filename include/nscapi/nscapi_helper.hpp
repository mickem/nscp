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
#include <string>

namespace nscapi {
namespace plugin_helper {
bool isNagiosReturnCode(NSCAPI::nagiosReturn code);
bool isMyNagiosReturn(NSCAPI::nagiosReturn code);
NSCAPI::nagiosReturn int2nagios(int code);
int nagios2int(NSCAPI::nagiosReturn code);
void escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode);
void escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode);
int wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode);
//		static NSCAPI_EXPORT std::wstring translateMessageType(NSCAPI::messageTypes msgType);
std::string translateReturn(NSCAPI::nagiosReturn returnCode);
NSCAPI::nagiosReturn translateReturn(std::string str);
NSCAPI::nagiosReturn maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b);
};  // namespace plugin_helper

namespace report {
unsigned int parse(std::string str);
bool matches(unsigned int report, NSCAPI::nagiosReturn code);
std::string to_string(unsigned int report);
}  // namespace report
namespace logging {
NSCAPI::log_level::level parse(std::string str);
bool matches(NSCAPI::log_level::level level, NSCAPI::nagiosReturn code);
std::string to_string(NSCAPI::log_level::level level);
}  // namespace logging
}  // namespace nscapi