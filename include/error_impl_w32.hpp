#pragma once
#include <unicode_char.hpp>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <vector>

#include <strEx.h>
#include <utf8.hpp>

namespace error {
	struct helpers {
		static std::string failed(unsigned long err1, unsigned long err2 = 0) {
			if (err2 == 0)
				err2 = GetLastError();
			return "failed to lookup error code: " + strEx::s::xtos(err1) + " (reason: " + strEx::s::xtos(err2) + ")";
		}
	};
	class format {
	public:

		static std::string from_system(unsigned long dwError) {
			LPVOID lpMsgBuf = NULL;
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
			if (dwRet == 0) {
				return error::helpers::failed(dwError);
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, _T("%d: %s"), dwError, lpMsgBuf); 
			std::string str = utf8::cvt<std::string>(std::wstring(szBuf));
			delete [] szBuf;
			LocalFree(lpMsgBuf);
			return str;
		}
		static std::string from_module(std::string module, unsigned long dwError) {
			LPVOID lpMsgBuf;
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS,
				GetModuleHandle(utf8::cvt<std::wstring>(module).c_str()),dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
			if (dwRet == 0) {
				return error::helpers::failed(dwError);
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, _T("%x: %s"), dwError, lpMsgBuf); 
			std::string str = utf8::cvt<std::string>(std::wstring(szBuf));
			delete [] szBuf;
			LocalFree(lpMsgBuf);
			return str;
		}
		static std::string from_module(std::string module, unsigned long dwError, DWORD *arguments) {
			LPVOID lpMsgBuf;
			HMODULE hevt = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
			if (hevt == NULL) {
				return error::helpers::failed(dwError);
			}
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY,hevt,
				dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,reinterpret_cast<va_list*>(arguments));
			if (dwRet == 0) {
				FreeLibrary(hevt);
				return error::helpers::failed(dwError);
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, _T("%d: %s"), dwError, lpMsgBuf); 
			std::string str = utf8::cvt<std::string>(std::wstring(szBuf));
			delete [] szBuf;
			LocalFree(lpMsgBuf);
			FreeLibrary(hevt);
			return str;
		}
		class message {
		public:
			static std::string from_module(std::string module, unsigned long dwError) {
				HMODULE hDLL = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					return error::helpers::failed(dwError);
				}
				LPVOID lpMsgBuf;
				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS,hDLL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
				if (dwRet == 0) {
					FreeLibrary(hDLL);
					DWORD err = GetLastError();
					if (err == 317) {
						return "";
					}
					return error::helpers::failed(dwError, err);
				}
				std::string str = utf8::cvt<std::string>(reinterpret_cast<wchar_t*>(lpMsgBuf));
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}

		private:
			static std::string from_module_wrapper(std::string module, unsigned long dwError, ...) {
				va_list Ellipsis;
				va_start(Ellipsis, dwError);
				std::string ret = __from_module(module, dwError, Ellipsis);
				va_end(Ellipsis);
				return ret;
			}
			static std::string __from_module(std::string module, unsigned long dwError, std::vector<std::wstring> strings) {
				if (strings.size() > 0)
					return __from_module(module, dwError, reinterpret_cast<va_list>(&strings[0]));
				return __from_module(module, dwError);
			}

			static std::string __from_module(std::string module, unsigned long dwError, va_list arguments) {
				HMODULE hDLL = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					return error::helpers::failed(dwError);
				}
				LPVOID lpMsgBuf;

				//unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY,hDLL,
				//	dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,reinterpret_cast<va_list*>(arguments));
				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE,hDLL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,reinterpret_cast<va_list*>(&arguments));
				if (dwRet == 0) {
					FreeLibrary(hDLL);
					DWORD err = GetLastError();
					if (err == 317) {
						return "";
					}
					return error::helpers::failed(dwError, err);
				}
				std::string str = utf8::cvt<std::string>(reinterpret_cast<wchar_t*>(lpMsgBuf));
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}
			static std::string __from_module(std::string module, unsigned long dwError) {
				HMODULE hDLL = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					return error::helpers::failed(dwError);
				}
				LPVOID lpMsgBuf;

				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE,hDLL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
				if (dwRet == 0) {
					FreeLibrary(hDLL);
					DWORD err = GetLastError();
					if (err == 317) {
						return "";
					}
					return error::helpers::failed(dwError, err);
				}
				std::string str = utf8::cvt<std::string>(reinterpret_cast<wchar_t*>(lpMsgBuf));
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}
		public:
			static std::string from_system(unsigned long dwError, DWORD *arguments) {
				LPVOID lpMsgBuf;
				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY,NULL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,reinterpret_cast<va_list*>(arguments));
				if (dwRet == 0) {
					DWORD err = GetLastError();
					if (err == 317) {
						return "";
					}
					return error::helpers::failed(dwError, err);
				}
				std::string str = utf8::cvt<std::string>(reinterpret_cast<wchar_t*>(lpMsgBuf));
				LocalFree(lpMsgBuf);
				return str;
			}
		};
	};
	class lookup {
	public:
		static std::string last_error(unsigned long dwLastError = -1) {
			if (dwLastError == -1) {
				dwLastError = GetLastError();
			}
			return ::error::format::from_system(dwLastError);
		}
	};
}
