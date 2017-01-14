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

#include <error/error_w32.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <str/xtos.hpp>
#include <utf8.hpp>

namespace error {
	namespace win32 {
    
		unsigned int lookup() {
			return GetLastError();
		}
    
		std::string failed(unsigned long err1, unsigned long err2) {
			if (err2 == 0)
				err2 = GetLastError();
			return "failed to lookup error code: " + str::xtos(err1) + " (reason: " + str::xtos(err2) + ")";
		}

		std::string format_message(unsigned long attrs, std::string module, unsigned long dwError) {
			LPVOID lpMsgBuf;
			HMODULE hMod = NULL;
			attrs |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
			if (!module.empty()) {
				attrs |= FORMAT_MESSAGE_FROM_HMODULE;
				hMod = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hMod == NULL) {
					return failed(dwError);
				}
			} else {
				attrs |= FORMAT_MESSAGE_FROM_SYSTEM;
			}
			unsigned long dwRet = FormatMessage(attrs, hMod, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
			if (dwRet == 0) {
				FreeLibrary(hMod);
				DWORD err = GetLastError();
				if (err == ERROR_MR_MID_NOT_FOUND) {
					return "";
				}
				return failed(dwError, err);
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, L"%x: %s", dwError, lpMsgBuf);
			std::string str = utf8::cvt<std::string>(std::wstring(szBuf));
			delete[] szBuf;
			LocalFree(lpMsgBuf);
			FreeLibrary(hMod);
			return str;
		}
		std::string format_message(unsigned long attrs, std::string module, unsigned long dwError, DWORD *arguments) {
			LPVOID lpMsgBuf;
			HMODULE hMod = NULL;
			attrs |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
			attrs |= FORMAT_MESSAGE_ARGUMENT_ARRAY;
			if (!module.empty()) {
				attrs |= FORMAT_MESSAGE_FROM_HMODULE;
				hMod = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hMod == NULL) {
					return failed(dwError);
				}
			} else {
				attrs |= FORMAT_MESSAGE_FROM_SYSTEM;
			}
			unsigned long dwRet = FormatMessage(attrs, hMod, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, reinterpret_cast<va_list*>(arguments));
			if (dwRet == 0) {
				FreeLibrary(hMod);
				return failed(dwError);
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, L"%d: %s", dwError, static_cast<wchar_t*>(lpMsgBuf));
			std::string str = utf8::cvt<std::string>(std::wstring(szBuf));
			delete[] szBuf;
			LocalFree(lpMsgBuf);
			FreeLibrary(hMod);
			return str;
		}
	};
}
