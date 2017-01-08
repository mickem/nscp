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

#include <windows.h>

#include <sysinfo.h>
#include <tchar.h>
#include <error/error.hpp>

namespace systemInfo {
	LANGID GetSystemDefaultUILanguage() {
		HMODULE hKernel = ::LoadLibrary(_TEXT("KERNEL32"));
		if (!hKernel)
			throw SystemInfoException("Could not load kernel32.dll: " + error::lookup::last_error());
		tGetSystemDefaultUILanguage fGetSystemDefaultUILanguage;
		fGetSystemDefaultUILanguage = (tGetSystemDefaultUILanguage)::GetProcAddress(hKernel, "GetSystemDefaultUILanguage");
		if (!fGetSystemDefaultUILanguage)
			throw SystemInfoException("Could not load GetSystemDefaultUILanguage" + error::lookup::last_error());
		return fGetSystemDefaultUILanguage();
	}
}