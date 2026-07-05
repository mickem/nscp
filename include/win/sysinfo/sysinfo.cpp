// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <tchar.h>
#include <win/sysinfo/sysinfo.h>

#include <error/error.hpp>
#include <win/windows.hpp>

namespace systemInfo {
LANGID GetSystemDefaultUILanguage() {
  HMODULE hKernel = ::LoadLibrary(_TEXT("KERNEL32"));
  if (!hKernel) throw SystemInfoException("Could not load kernel32.dll: " + error::lookup::last_error());
  tGetSystemDefaultUILanguage fGetSystemDefaultUILanguage;
  fGetSystemDefaultUILanguage = (tGetSystemDefaultUILanguage)::GetProcAddress(hKernel, "GetSystemDefaultUILanguage");
  if (!fGetSystemDefaultUILanguage) throw SystemInfoException("Could not load GetSystemDefaultUILanguage" + error::lookup::last_error());
  return fGetSystemDefaultUILanguage();
}
}  // namespace systemInfo