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

#include <NSCAPI.h>

namespace nscapi {

	class plugin_helper {
	public:
		static bool isNagiosReturnCode(NSCAPI::nagiosReturn code);
		static bool isMyNagiosReturn(NSCAPI::nagiosReturn code);
		static NSCAPI::nagiosReturn int2nagios(int code);
		static int nagios2int(NSCAPI::nagiosReturn code);
		static void escalteReturnCodeToCRIT(NSCAPI::nagiosReturn &currentReturnCode);
		static void escalteReturnCodeToWARN(NSCAPI::nagiosReturn &currentReturnCode);
		//static int wrapReturnString(wchar_t *buffer, unsigned int bufLen, std::wstring str, int defaultReturnCode );
		static int wrapReturnString(char *buffer, unsigned int bufLen, std::string str, int defaultReturnCode );
		static std::wstring translateMessageType(NSCAPI::messageTypes msgType);
		static std::string translateReturn(NSCAPI::nagiosReturn returnCode);
		static NSCAPI::nagiosReturn translateReturn(std::string str);
		static NSCAPI::nagiosReturn maxState(NSCAPI::nagiosReturn a, NSCAPI::nagiosReturn b);
	};

	namespace report {
		unsigned int parse(std::string str);
		bool matches(unsigned int report, NSCAPI::nagiosReturn code);
		std::wstring to_string(unsigned int report);
	}
	namespace logging {
		NSCAPI::log_level::level parse(std::string str);
		bool matches(NSCAPI::log_level::level level, NSCAPI::nagiosReturn code);
		std::string to_string(NSCAPI::log_level::level level);
	}
}
