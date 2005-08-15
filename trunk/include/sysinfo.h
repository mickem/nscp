#pragma once

#include <windows.h>
#include <string>

namespace systemInfo {
	struct SystemInfoException {
		std::string error_;
		SystemInfoException(std::string error, DWORD code) : error_(error){}

	};
	typedef LANGID (*tGetSystemDefaultUILanguage)(void);


	inline LANGID GetSystemDefaultLangID() {
		return ::GetSystemDefaultLangID();
	}

	LANGID GetSystemDefaultUILanguage();
	inline OSVERSIONINFO getOSVersion() {
		OSVERSIONINFO OSversion;
		OSversion.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
		::GetVersionEx(&OSversion);
		return OSversion;
	}

	inline bool isNTBased(const OSVERSIONINFO &osVersion) {
		return osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT;
	}
	inline bool isBelowNT4(const OSVERSIONINFO &osVersion) {
		return ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT)&&(osVersion.dwMajorVersion<=4));
	}
	inline bool isAboveW2K(const OSVERSIONINFO &osVersion) {
		return ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT)&&(osVersion.dwMajorVersion>4));
	}

}