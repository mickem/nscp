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

#include <nscp_string.hpp>

#include <nscapi/nscapi_helper.hpp>

#define REPORT_ERROR	0x01
#define REPORT_WARNING	0x02
#define REPORT_UNKNOWN	0x04
#define REPORT_OK		0x08

unsigned int nscapi::report::parse(std::string str) {
	unsigned int report = 0;
	BOOST_FOREACH(const std::string &key, strEx::s::splitEx(str, std::string(","))) {
		if (key == "all") {
			report |= REPORT_ERROR|REPORT_OK|REPORT_UNKNOWN|REPORT_WARNING;
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
		(code == NSCAPI::returnOK && ((report&REPORT_OK)==REPORT_OK) ) ||
		(code == NSCAPI::returnCRIT && ((report&REPORT_ERROR)==REPORT_ERROR) ) ||
		(code == NSCAPI::returnWARN && ((report&REPORT_WARNING)==REPORT_WARNING) ) ||
		(code == NSCAPI::returnUNKNOWN && ((report&REPORT_UNKNOWN)==REPORT_UNKNOWN) ) ||
		( (code != NSCAPI::returnOK) && (code != NSCAPI::returnCRIT) && (code != NSCAPI::returnWARN) && (code != NSCAPI::returnUNKNOWN) )
		);
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
int nscapi::plugin_helper::wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode ) {
	// @todo deprecate this
	if (str.length() >= bufLen) {
		return NSCAPI::isInvalidBufferLen;
	}
	strncpy(buffer, str.c_str(), bufLen);
	return defaultReturnCode;
}

bool nscapi::plugin_helper::isNagiosReturnCode(NSCAPI::nagiosReturn code) {
	return ( (code == NSCAPI::returnOK) || (code == NSCAPI::returnWARN) || (code == NSCAPI::returnCRIT) || (code == NSCAPI::returnUNKNOWN) );
}
bool nscapi::plugin_helper::isMyNagiosReturn(NSCAPI::nagiosReturn code) {
	return code == NSCAPI::returnCRIT || code == NSCAPI::returnOK || code == NSCAPI::returnWARN || code == NSCAPI::returnUNKNOWN  || code == NSCAPI::returnInvalidBufferLen || code == NSCAPI::returnIgnored;
}
NSCAPI::nagiosReturn nscapi::plugin_helper::int2nagios(int code) {
	return code;
}
int nscapi::plugin_helper::nagios2int(NSCAPI::nagiosReturn code) {
	return code;
}
void nscapi::plugin_helper::escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode) {
	currentReturnCode = NSCAPI::returnCRIT;
}
void nscapi::plugin_helper::escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode) {
	if (currentReturnCode != NSCAPI::returnCRIT)
		currentReturnCode = NSCAPI::returnWARN;
}

/**
* Translate a return code into the corresponding string
* @param returnCode
* @return
*/
std::string nscapi::plugin_helper::translateReturn(NSCAPI::nagiosReturn returnCode) {
	if (returnCode == NSCAPI::returnOK)
		return "OK";
	else if (returnCode == NSCAPI::returnCRIT)
		return "CRITICAL";
	else if (returnCode == NSCAPI::returnWARN)
		return "WARNING";
	else if (returnCode == NSCAPI::returnUNKNOWN)
		return "UNKNOWN";
	else
		return "BAD_CODE: " + strEx::s::xtos(returnCode);
}
/**
* Translate a string into the corresponding return code
* @param returnCode
* @return
*/
NSCAPI::nagiosReturn nscapi::plugin_helper::translateReturn(std::string str) {
	if ((str == "OK") || (str == "ok"))
		return NSCAPI::returnOK;
	else if ((str == "CRITICAL") || (str == "critical"))
		return NSCAPI::returnCRIT;
	else if ((str == "WARNING") || (str == "warning"))
		return NSCAPI::returnWARN;
	else
		return NSCAPI::returnUNKNOWN;
}
/**
* Returns the biggest of the two states
* STATE_UNKNOWN < STATE_OK < STATE_WARNING < STATE_CRITICAL
* @param a
* @param b
* @return
*/
NSCAPI::nagiosReturn nscapi::plugin_helper::maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b) {
	if (a == NSCAPI::returnCRIT || b == NSCAPI::returnCRIT)
		return NSCAPI::returnCRIT;
	else if (a == NSCAPI::returnWARN || b == NSCAPI::returnWARN)
		return NSCAPI::returnWARN;
	else if (a == NSCAPI::returnOK || b == NSCAPI::returnOK)
		return NSCAPI::returnOK;
	else if (a == NSCAPI::returnUNKNOWN || b == NSCAPI::returnUNKNOWN)
		return NSCAPI::returnUNKNOWN;
	return NSCAPI::returnUNKNOWN;
}