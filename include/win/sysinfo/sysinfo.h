// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <win/windows.hpp>

namespace systemInfo {
class SystemInfoException {
  std::string error_;

 public:
  SystemInfoException(std::string error) : error_(error) {}
  std::string reason() const { return error_; }
};
typedef LANGID (*tGetSystemDefaultUILanguage)(void);

inline LANGID GetSystemDefaultLangID() { return ::GetSystemDefaultLangID(); }

LANGID GetSystemDefaultUILanguage();
#pragma warning(disable : 4996)
inline OSVERSIONINFO getOSVersion() {
  OSVERSIONINFO OSversion;
  OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  ::GetVersionEx(&OSversion);
  return OSversion;
}
#pragma warning(default : 4996)

inline bool isNTBased(const OSVERSIONINFO &osVersion) { return osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT; }
inline bool isBelowNT4(const OSVERSIONINFO &osVersion) { return ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion <= 4)); }
inline bool isAboveW2K(const OSVERSIONINFO &osVersion) { return ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion > 4)); }
inline bool isAboveXP(const OSVERSIONINFO &osVersion) {
  if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion == 5) && (osVersion.dwMinorVersion >= 1)) return true;
  if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion > 5)) return true;
  return false;
}
inline bool isAboveVista(const OSVERSIONINFO &osVersion) { return osVersion.dwMajorVersion >= 6; }
inline bool isBelowXP(const OSVERSIONINFO &osVersion) {
  if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion < 4)) return true;
  if ((osVersion.dwPlatformId == VER_PLATFORM_WIN32_NT) && (osVersion.dwMajorVersion == 4) && (osVersion.dwMinorVersion < 1)) return true;
  return false;
}
}  // namespace systemInfo