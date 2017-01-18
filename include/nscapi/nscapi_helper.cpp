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

#include <nscapi/nscapi_helper.hpp>

#include <str/xtos.hpp>
#include <str/utils.hpp>

#include <boost/foreach.hpp>

#define REPORT_ERROR	0x01
#define REPORT_WARNING	0x02
#define REPORT_UNKNOWN	0x04
#define REPORT_OK		0x08

unsigned int nscapi::report::parse(std::string str) {
	unsigned int report = 0;
	BOOST_FOREACH(const std::string &key, str::utils::split_lst(str, std::string(","))) {
		if (key == "all") {
			report |= REPORT_ERROR | REPORT_OK | REPORT_UNKNOWN | REPORT_WARNING;
		} else if (key == "error" || key == "err" || key == "critical" || key == "crit") {
			report |= REPORT_ERROR;
		} else if (key == "warning" || key == "warn") {
			report |= REPORT_WARNING;
		} else if (key == "unknown") {
			report |= REPORT_UNKNOWN;
		} else if (key == "ok") {
			report |= REPORT_OK;
		}
	}
	return report;
}
bool nscapi::report::matches(unsigned int report, NSCAPI::nagiosReturn code) {
	return (
		(code == NSCAPI::query_return_codes::returnOK && ((report&REPORT_OK) == REPORT_OK)) ||
		(code == NSCAPI::query_return_codes::returnCRIT && ((report&REPORT_ERROR) == REPORT_ERROR)) ||
		(code == NSCAPI::query_return_codes::returnWARN && ((report&REPORT_WARNING) == REPORT_WARNING)) ||
		(code == NSCAPI::query_return_codes::returnUNKNOWN && ((report&REPORT_UNKNOWN) == REPORT_UNKNOWN)) ||
		((code != NSCAPI::query_return_codes::returnOK) && (code != NSCAPI::query_return_codes::returnCRIT) && (code != NSCAPI::query_return_codes::returnWARN) && (code != NSCAPI::query_return_codes::returnUNKNOWN))
		);
}

std::string nscapi::report::to_string(unsigned int report) {
	std::string ret;
	if ((report&REPORT_OK) == REPORT_OK) {
		ret += "ok";
	}
	if ((report&REPORT_ERROR) == REPORT_ERROR) {
		if (!ret.empty()) ret += ",";
		ret += "crit";
	}
	if ((report&REPORT_WARNING) == REPORT_WARNING) {
		if (!ret.empty()) ret += ",";
		ret += "warn,";
	}
	if ((report&REPORT_UNKNOWN) == REPORT_UNKNOWN) {
		if (!ret.empty()) ret += ",";
		ret += "unknown,";
	}
	if (ret.empty()) ret = "<none>";
	return ret;
}

NSCAPI::log_level::level nscapi::logging::parse(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	if ("all" == str) {
		return NSCAPI::log_level::trace;
	} else if ("error" == str) {
		return NSCAPI::log_level::error;
	} else if ("critical" == str) {
		return NSCAPI::log_level::critical;
	} else if ("debug" == str) {
		return NSCAPI::log_level::debug;
	} else if ("trace" == str) {
		return NSCAPI::log_level::trace;
	} else if ("info" == str) {
		return NSCAPI::log_level::info;
	} else if ("warning" == str) {
		return NSCAPI::log_level::warning;
	} else if ("off" == str) {
		return NSCAPI::log_level::off;
	}
	return NSCAPI::log_level::unknown;
}
bool nscapi::logging::matches(NSCAPI::log_level::level level, NSCAPI::nagiosReturn code) {
	return code <= level;
}

std::string nscapi::logging::to_string(NSCAPI::log_level::level level) {
	switch (level) {
	case NSCAPI::log_level::trace:
		return "trace";
	case NSCAPI::log_level::error:
		return "error";
	case NSCAPI::log_level::critical:
		return "critical";
	case NSCAPI::log_level::debug:
		return "debug";
	case NSCAPI::log_level::info:
		return "info";
	case NSCAPI::log_level::warning:
		return "warning";
	case NSCAPI::log_level::off:
		return "off";
	}
	return "unknown";
}

/**
* Wrap a return string.
* This function copies a string to a char buffer making sure the buffer has the correct length.
*
* @param *buffer Buffer to copy the string to.
* @param bufLen Length of the buffer
* @param str Th string to copy
* @param defaultReturnCode The default return code
* @return NSCAPI::success unless the buffer is to short then it will be NSCAPI::invalidBufferLen
*/
int nscapi::plugin_helper::wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode) {
	// @todo deprecate this
	if (str.length() >= bufLen) {
		return NSCAPI::isInvalidBufferLen;
	}
	strncpy(buffer, str.c_str(), bufLen);
	return defaultReturnCode;
}

bool nscapi::plugin_helper::isNagiosReturnCode(NSCAPI::nagiosReturn code) {
	return ((code == NSCAPI::query_return_codes::returnOK) || (code == NSCAPI::query_return_codes::returnWARN) || (code == NSCAPI::query_return_codes::returnCRIT) || (code == NSCAPI::query_return_codes::returnUNKNOWN));
}
bool nscapi::plugin_helper::isMyNagiosReturn(NSCAPI::nagiosReturn code) {
	return code == NSCAPI::query_return_codes::returnCRIT || code == NSCAPI::query_return_codes::returnOK || code == NSCAPI::query_return_codes::returnWARN || code == NSCAPI::query_return_codes::returnUNKNOWN;
}
NSCAPI::nagiosReturn nscapi::plugin_helper::int2nagios(int code) {
	return code;
}
int nscapi::plugin_helper::nagios2int(NSCAPI::nagiosReturn code) {
	return code;
}
void nscapi::plugin_helper::escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode) {
	currentReturnCode = NSCAPI::query_return_codes::returnCRIT;
}
void nscapi::plugin_helper::escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode) {
	if (currentReturnCode != NSCAPI::query_return_codes::returnCRIT)
		currentReturnCode = NSCAPI::query_return_codes::returnWARN;
}

/**
* Translate a return code into the corresponding string
* @param returnCode
* @return
*/
std::string nscapi::plugin_helper::translateReturn(NSCAPI::nagiosReturn returnCode) {
	if (returnCode == NSCAPI::query_return_codes::returnOK)
		return "OK";
	else if (returnCode == NSCAPI::query_return_codes::returnCRIT)
		return "CRITICAL";
	else if (returnCode == NSCAPI::query_return_codes::returnWARN)
		return "WARNING";
	else if (returnCode == NSCAPI::query_return_codes::returnUNKNOWN)
		return "UNKNOWN";
	else
		return "BAD_CODE: " + str::xtos(returnCode);
}
/**
* Translate a string into the corresponding return code
* @param returnCode
* @return
*/
NSCAPI::nagiosReturn nscapi::plugin_helper::translateReturn(std::string str) {
	if ((str == "OK") || (str == "ok"))
		return NSCAPI::query_return_codes::returnOK;
	else if ((str == "CRITICAL") || (str == "critical"))
		return NSCAPI::query_return_codes::returnCRIT;
	else if ((str == "WARNING") || (str == "warning"))
		return NSCAPI::query_return_codes::returnWARN;
	else
		return NSCAPI::query_return_codes::returnUNKNOWN;
}
/**
* Returns the biggest of the two states
* STATE_OK < STATE_WARNING < STATE_CRITICAL < STATE_UNKNOWN
* @param a
* @param b
* @return
*/
NSCAPI::nagiosReturn nscapi::plugin_helper::maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b) {
	if (a == NSCAPI::query_return_codes::returnUNKNOWN || b == NSCAPI::query_return_codes::returnUNKNOWN)
		return NSCAPI::query_return_codes::returnUNKNOWN;
	else if (a == NSCAPI::query_return_codes::returnCRIT || b == NSCAPI::query_return_codes::returnCRIT)
		return NSCAPI::query_return_codes::returnCRIT;
	else if (a == NSCAPI::query_return_codes::returnWARN || b == NSCAPI::query_return_codes::returnWARN)
		return NSCAPI::query_return_codes::returnWARN;
	else if (a == NSCAPI::query_return_codes::returnOK || b == NSCAPI::query_return_codes::returnOK)
		return NSCAPI::query_return_codes::returnOK;
	return NSCAPI::query_return_codes::returnUNKNOWN;
}