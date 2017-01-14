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