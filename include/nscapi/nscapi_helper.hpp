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

namespace nscapi {

	class plugin_helper {
	public:
		static bool isNagiosReturnCode(NSCAPI::nagiosReturn code) {
			return ( (code == NSCAPI::returnOK) || (code == NSCAPI::returnWARN) || (code == NSCAPI::returnCRIT) || (code == NSCAPI::returnUNKNOWN) );
		}
		static bool isMyNagiosReturn(NSCAPI::nagiosReturn code) {
			return code == NSCAPI::returnCRIT || code == NSCAPI::returnOK || code == NSCAPI::returnWARN || code == NSCAPI::returnUNKNOWN  || code == NSCAPI::returnInvalidBufferLen || code == NSCAPI::returnIgnored;
		}
		static NSCAPI::nagiosReturn int2nagios(int code) {
			return code;
		}
		static int nagios2int(NSCAPI::nagiosReturn code) {
			return code;
		}
		static void escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode) {
			currentReturnCode = NSCAPI::returnCRIT;
		}
		static void escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode) {
			if (currentReturnCode != NSCAPI::returnCRIT)
				currentReturnCode = NSCAPI::returnWARN;
		}
		static int wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode );

		/**
		 * Translate a message type into a human readable string.
		 *
		 * @param msgType The message type
		 * @return A string representing the message type
		 */
		static std::wstring translateMessageType(NSCAPI::messageTypes msgType) {
			switch (msgType) {
				case NSCAPI::error:
					return _T("error");
				case NSCAPI::critical:
					return _T("critical");
				case NSCAPI::warning:
					return _T("warning");
				case NSCAPI::log:
					return _T("message");
				case NSCAPI::debug:
					return _T("debug");
			}
			return _T("unknown");
		}
		/**
		 * Translate a return code into the corresponding string
		 * @param returnCode 
		 * @return 
		 */
		static std::wstring translateReturn(NSCAPI::nagiosReturn returnCode) {
			if (returnCode == NSCAPI::returnOK)
				return _T("OK");
			else if (returnCode == NSCAPI::returnCRIT)
				return _T("CRITICAL");
			else if (returnCode == NSCAPI::returnWARN)
				return _T("WARNING");
			else if (returnCode == NSCAPI::returnUNKNOWN)
				return _T("WARNING");
			else
				return _T("BAD_CODE");
		}
		/**
		* Translate a string into the corresponding return code 
		* @param returnCode 
		* @return 
		*/
		static NSCAPI::nagiosReturn translateReturn(std::wstring str) {
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
		static NSCAPI::nagiosReturn maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b) {
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

	};

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
