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

#include <nscapi/nscapi_helper.hpp>
#include <nscapi/macros.hpp>
#include <msvc_wrappers.h>
#include <settings/macros.h>
#include <arrayBuffer.h>
//#include <config.h>
#include <strEx.h>

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

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
/*
int NSCHelper::wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode ) {
	// @todo deprecate this
	if (str.length() >= bufLen) {
		std::wstring sstr = str.substr(0, bufLen-2);
		NSC_DEBUG_MSG_STD(_T("String (") + strEx::itos(str.length()) + _T(") to long to fit inside buffer(") + strEx::itos(bufLen) + _T(") : ") + sstr);
		return NSCAPI::isInvalidBufferLen;
	}
	wcsncpy_s(buffer, bufLen, str.c_str(), bufLen);
	return defaultReturnCode;
}
*/


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

#define LOG_CRIT	0x10
#define LOG_ERROR	0x08
#define LOG_WARNING	0x04
#define LOG_MSG		0x02
#define LOG_DEBUG	0x01

unsigned int nscapi::logging::parse(std::wstring str) {
	unsigned int report = 0;
	strEx::splitList lst = strEx::splitEx(str, _T(","));

	for (strEx::splitList::const_iterator key = lst.begin(); key != lst.end(); ++key) {
		if (*key == _T("all")) {
			report |= LOG_MSG|LOG_ERROR|LOG_CRIT|LOG_WARNING|LOG_DEBUG;
		} else if (*key == _T("normal")) {
				report |= LOG_MSG|LOG_ERROR|LOG_CRIT|LOG_WARNING;
		} else if (*key == _T("log") || *key == _T("message") || *key == _T("info") || *key == _T("INFO")) {
			report |= LOG_MSG;
		} else if (*key == _T("error") || *key == _T("ERROR")) {
			report |= LOG_ERROR;
		} else if (*key == _T("critical") || *key == _T("CRITICAL")) {
			report |= LOG_CRIT;
		} else if (*key == _T("warning") || *key == _T("WARN")) {
			report |= LOG_WARNING;
		} else if (*key == _T("debug") || *key == _T("DEBUG")) {
			report |= LOG_DEBUG;
		}
	}
	return report;
}
bool nscapi::logging::matches(unsigned int report, NSCAPI::nagiosReturn code) {
	return (
		(code == NSCAPI::critical && ((report&LOG_CRIT)==LOG_CRIT) ) ||
		(code == NSCAPI::error && ((report&LOG_ERROR)==LOG_ERROR) ) ||
		(code == NSCAPI::warning && ((report&LOG_WARNING)==LOG_WARNING) ) ||
		(code == NSCAPI::log && ((report&LOG_MSG)==LOG_MSG) ) ||
		(code == NSCAPI::debug && ((report&LOG_DEBUG)==LOG_DEBUG) ) ||
		( (code != NSCAPI::critical) && (code != NSCAPI::error) && (code != NSCAPI::warning) && (code != NSCAPI::log) && (code != NSCAPI::debug) )
		);
}

std::wstring nscapi::logging::to_string(unsigned int report) {
	std::wstring ret;
	if ((report&LOG_CRIT)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("critical");
	}
	if ((report&LOG_ERROR)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("error");
	}
	if ((report&LOG_WARNING)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("warning");
	}
	if ((report&LOG_MSG)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("message");
	}
	if ((report&LOG_DEBUG)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("debug");
	}
	return ret;
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
	wcsncpy_s(buffer, bufLen, str.c_str(), bufLen);
	return defaultReturnCode;
}
