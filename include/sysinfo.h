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