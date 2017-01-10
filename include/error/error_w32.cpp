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
