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

#include <strEx.h>

#include <nscapi/nscapi_helper.hpp>

#define REPORT_ERROR	0x01
#define REPORT_WARNING	0x02
#define REPORT_UNKNOWN	0x04
#define REPORT_OK		0x08

unsigned int nscapi::report::parse(std::wstring str) {
	unsigned int report = 0;
	strEx::splitList lst = strEx::splitEx(str, _T(","));
	for (strEx::splitList::const_iterator key = lst.begin(); key != lst.end(); ++key) {
		if (*key == _T("all")) {
			report |= REPORT_ERROR|REPORT_OK|REPORT_UNKNOWN|REPORT_WARNING;
		} else if (*key == _T("error") || *key == _T("err") || *key == _T("critical") || *key == _T("crit")) {
			report |= REPORT_ERROR;
		} else if (*key == _T("warning") || *key == _T("warn")) {
			report |= REPORT_WARNING;
		} else if (*key == _T("unknown")) {
			report |= REPORT_UNKNOWN;
		} else if (*key == _T("ok")) {
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


std::wstring nscapi::report::to_string(unsigned int report) {
	std::wstring ret;
	if ((report&REPORT_OK)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("ok");
	}
	if ((report&REPORT_WARNING)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("warning");
	}
	if ((report&REPORT_ERROR)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("critical");
	}
	if ((report&REPORT_UNKNOWN)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("unknown");
	}
	return ret;
}

#define PARSE_LOGLEVEL_BEGIN() if (false) {}
#define PARSE_LOGLEVEL(key_str, value)  else if (*key == _T(key_str) && level < value) { level = value; }
#define PARSE_LOGLEVEL_END() 

NSCAPI::log_level::level nscapi::logging::parse(std::wstring str) {
	unsigned int level = 0;
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	strEx::splitList lst = strEx::splitEx(str, _T(","));
	for (strEx::splitList::const_iterator key = lst.begin(); key != lst.end(); ++key) {
		PARSE_LOGLEVEL_BEGIN()
			PARSE_LOGLEVEL("all",		NSCAPI::log_level::trace)
			PARSE_LOGLEVEL("error",		NSCAPI::log_level::error)
			PARSE_LOGLEVEL("critical",	NSCAPI::log_level::critical)
			PARSE_LOGLEVEL("debug",		NSCAPI::log_level::debug)
			PARSE_LOGLEVEL("trace",		NSCAPI::log_level::trace)
			PARSE_LOGLEVEL("info",		NSCAPI::log_level::info)
			PARSE_LOGLEVEL("warning",	NSCAPI::log_level::warning)
		PARSE_LOGLEVEL_END()
	}
	return level;
}
bool nscapi::logging::matches(NSCAPI::log_level::level level, NSCAPI::nagiosReturn code) {
	return code <= level;
}

#define RENDER_LOGLEVEL_BEGIN() 
#define RENDER_LOGLEVEL(key_str, value)  if (level == value) { return _T(key_str); }
#define RENDER_LOGLEVEL_END() 

std::wstring nscapi::logging::to_string(NSCAPI::log_level::level level) {
	RENDER_LOGLEVEL_BEGIN()
		RENDER_LOGLEVEL("all",		NSCAPI::log_level::trace)
		RENDER_LOGLEVEL("error",		NSCAPI::log_level::error)
		RENDER_LOGLEVEL("critical",	NSCAPI::log_level::critical)
		RENDER_LOGLEVEL("debug",		NSCAPI::log_level::debug)
		RENDER_LOGLEVEL("trace",		NSCAPI::log_level::trace)
		RENDER_LOGLEVEL("info",		NSCAPI::log_level::info)
		RENDER_LOGLEVEL("warning",	NSCAPI::log_level::warning)
	RENDER_LOGLEVEL_END()
	return strEx::itos(level);
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
int nscapi::plugin_helper::wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode ) {
	// @todo deprecate this
	if (str.length() >= bufLen) {
		std::wstring sstr = str.substr(0, bufLen-2);
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy(buffer, str.c_str(), bufLen);
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
 * Translate a message type into a human readable string.
 *
 * @param msgType The message type
 * @return A string representing the message type
 */
std::wstring nscapi::plugin_helper::translateMessageType(NSCAPI::messageTypes msgType) {
	switch (msgType) {
		case NSCAPI::log_level::error:
			return _T("error");
		case NSCAPI::log_level::critical:
			return _T("critical");
		case NSCAPI::log_level::warning:
			return _T("warning");
		case NSCAPI::log_level::info:
			return _T("info");
		case NSCAPI::log_level::trace:
			return _T("trace");
		case NSCAPI::log_level::debug:
			return _T("debug");
	}
	return _T("unknown");
}
/**
 * Translate a return code into the corresponding string
 * @param returnCode 
 * @return 
 */
std::wstring nscapi::plugin_helper::translateReturn(NSCAPI::nagiosReturn returnCode) {
	if (returnCode == NSCAPI::returnOK)
		return _T("OK");
	else if (returnCode == NSCAPI::returnCRIT)
		return _T("CRITICAL");
	else if (returnCode == NSCAPI::returnWARN)
		return _T("WARNING");
	else if (returnCode == NSCAPI::returnUNKNOWN)
		return _T("WARNING");
	else
		return _T("BAD_CODE: " + strEx::itos(returnCode));
}
/**
* Translate a string into the corresponding return code 
* @param returnCode 
* @return 
*/
NSCAPI::nagiosReturn nscapi::plugin_helper::translateReturn(std::wstring str) {
	if (str == _T("OK"))
		return NSCAPI::returnOK;
	else if (str == _T("CRITICAL"))
		return NSCAPI::returnCRIT;
	else if (str == _T("WARNING"))
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
