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

namespace systemInfo {
	class SystemInfoException {
		std::string error_;
	public:
		SystemInfoException(std::string error) : error_(error) {}
		std::string reason() const {
			return error_;
		}
	};
	typedef LANGID(*tGetSystemDefaultUILanguage)(void);

	inline LANGID GetSystemDefaultLangID() {
		return ::GetSystemDefaultLangID();
	}

	LANGID GetSystemDefaultUILanguage();
	inline OSVERSIONINFO getOSVersion() {
		OSVERSIONINFO OSversion;
		OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		::GetVersionEx(&OSversion);
		return OSversion;
	}

	inline bool isNTBased(const OSVERSIONINFO &osVersion) {
		return osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT;
	}
	inline bool isBelowNT4(const OSVERSIONINFO &osVersion) {
		return ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion <= 4));
	}
	inline bool isAboveW2K(const OSVERSIONINFO &osVersion) {
		return ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion > 4));
	}
	inline bool isAboveXP(const OSVERSIONINFO &osVersion) {
		if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion == 5) && (osVersion.dwMinorVersion >= 1))
			return true;
		if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion > 5))
			return true;
		return false;
	}
	inline bool isAboveVista(const OSVERSIONINFO &osVersion) {
		return osVersion.dwMajorVersion >= 6;
	}
	inline bool isBelowXP(const OSVERSIONINFO &osVersion) {
		if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion < 4))
			return true;
		if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion == 4) && (osVersion.dwMinorVersion < 1))
			return true;
		return false;
	}
}