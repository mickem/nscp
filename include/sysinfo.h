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