// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <NSCAPI.h>

#include <nscapi/nscapi_helper.hpp>
#include <string>

namespace nscapi {
namespace plugin_helper {
bool isNagiosReturnCode(NSCAPI::nagiosReturn code);
bool isMyNagiosReturn(NSCAPI::nagiosReturn code);
NSCAPI::nagiosReturn int2nagios(int code);
int nagios2int(NSCAPI::nagiosReturn code);
void escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode);
void escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode);
// Promote OK to UNKNOWN; leave WARN/CRIT/UNKNOWN untouched. Used by
// modern_filter::match_post when the no-rows force-evaluate path produces
// an unsure verdict — i.e. the user's mixed expression depended on
// object-bound subterms that could not be resolved without an object.
void escalateReturnCodeToUNKNOWN(NSCAPI::nagiosReturn &currentReturnCode);
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