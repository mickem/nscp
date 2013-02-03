#pragma once
#include <unicode_char.hpp>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strEx.h>
#include <vector>

namespace error {
	class format {
	public:
		static std::wstring from_system(unsigned long dwError) {
			LPVOID lpMsgBuf = NULL;
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
			if (dwRet == 0) {
				return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, _T("%d: %s"), dwError, lpMsgBuf); 
			std::wstring str = szBuf;
			delete [] szBuf;
			LocalFree(lpMsgBuf);
			return str;
		}
		static std::wstring from_module(std::wstring module, unsigned long dwError) {
			LPVOID lpMsgBuf;
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS,
				GetModuleHandle(module.c_str()),dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
			if (dwRet == 0) {
				return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, _T("%d: %s"), dwError, lpMsgBuf); 
			std::wstring str = szBuf;
			delete [] szBuf;
			LocalFree(lpMsgBuf);
			return str;
		}
		static std::wstring from_module(std::wstring module, unsigned long dwError, DWORD *arguments) {
			LPVOID lpMsgBuf;
			HMODULE hevt = LoadLibraryEx(module.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
			if (hevt == NULL) {
				return _T("failed to load: ") + module + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
			}
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY,hevt,
				dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,reinterpret_cast<va_list*>(arguments));
			if (dwRet == 0) {
				FreeLibrary(hevt);
				return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T(" from DLL: ") + module + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
			}
			wchar_t *szBuf = new wchar_t[dwRet + 100];
			wsprintf(szBuf, _T("%d: %s"), dwError, lpMsgBuf); 
			std::wstring str = szBuf;
			delete [] szBuf;
			LocalFree(lpMsgBuf);
			FreeLibrary(hevt);
			return str;
		}
		class message {
		public:
			static std::wstring from_module(std::wstring module, unsigned long dwError) {
				HMODULE hDLL = LoadLibraryEx(module.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					return _T("failed to load: ") + module + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
				}
				LPVOID lpMsgBuf;
				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_IGNORE_INSERTS,hDLL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
				if (dwRet == 0) {
					FreeLibrary(hDLL);
					DWORD err = GetLastError();
					if (err == 317) {
						return _T("");
					}
					return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T(" from DLL: ") + module + _T("( reson: ") + strEx::itos(err) + _T(")");
				}
				std::wstring str = reinterpret_cast<wchar_t*>(lpMsgBuf);
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}

			static std::wstring from_module_x64(std::wstring module, unsigned long dwError, wchar_t* argList[], DWORD argCount) {
				if (argCount == 0)
					return from_module_wrapper(module, dwError);
				if (argCount == 1)
					return from_module_wrapper(module, dwError, argList[0]);
				if (argCount == 2)
					return from_module_wrapper(module, dwError, argList[0], argList[1]);
				if (argCount == 3)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2]);
				if (argCount == 4)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3]);
				if (argCount == 5)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3], argList[4]);
				if (argCount == 6)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3], argList[4], argList[5]);
				if (argCount == 7)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3], argList[4], argList[5], argList[6]);
				if (argCount == 8)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3], argList[4], argList[5], argList[6], argList[7]);
				if (argCount == 9)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3], argList[4], argList[5], argList[6], argList[7], argList[8]);
				if (argCount == 10)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3], argList[4], argList[5], argList[6], argList[7], argList[8], argList[9]);
				if (argCount == 11)
					return from_module_wrapper(module, dwError, argList[0], argList[1], argList[2], argList[3], argList[4], argList[5], argList[6], argList[7], argList[8], argList[9], argList[10]);
				return _T("We cant handle ") + strEx::itos(argCount) + _T(" arguments so you wont get argList here");
			}
			static std::wstring from_module_x64(std::wstring module, unsigned long dwError, std::vector<std::wstring> strings) {
				return __from_module(module, dwError, strings);
			}

		private:

			static std::wstring from_module_wrapper(std::wstring module, unsigned long dwError, ...) {
				va_list Ellipsis;
				va_start(Ellipsis, dwError);
				std::wstring ret = __from_module(module, dwError, Ellipsis);
				va_end(Ellipsis);
				return ret;
			}
			static std::wstring __from_module(std::wstring module, unsigned long dwError, std::vector<std::wstring> strings) {
				if (strings.size() > 0)
					return __from_module(module, dwError, reinterpret_cast<va_list>(&strings[0]));
				return __from_module(module, dwError);
			}

			static std::wstring __from_module(std::wstring module, unsigned long dwError, va_list arguments) {
				HMODULE hDLL = LoadLibraryEx(module.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					return _T("failed to load: ") + module + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
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
						return _T("");
					}
					return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T(" from DLL: ") + module + _T("( reson: ") + strEx::itos(err) + _T(")");
				}
				std::wstring str = reinterpret_cast<wchar_t*>(lpMsgBuf);
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}
			static std::wstring __from_module(std::wstring module, unsigned long dwError) {
				HMODULE hDLL = LoadLibraryEx(module.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					return _T("failed to load: ") + module + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
				}
				LPVOID lpMsgBuf;

				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE,hDLL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
				if (dwRet == 0) {
					FreeLibrary(hDLL);
					DWORD err = GetLastError();
					if (err == 317) {
						return _T("");
					}
					return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T(" from DLL: ") + module + _T("( reson: ") + strEx::itos(err) + _T(")");
				}
				std::wstring str = reinterpret_cast<wchar_t*>(lpMsgBuf);
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}
		public:
			static std::wstring from_system(unsigned long dwError, DWORD *arguments) {
				LPVOID lpMsgBuf;
				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY,NULL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,reinterpret_cast<va_list*>(arguments));
				if (dwRet == 0) {
					DWORD err = GetLastError();
					if (err == 317) {
						return _T("");
					}
					return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T(" from system ( reson: ") + strEx::itos(err) + _T(")");
				}
				std::wstring str = reinterpret_cast<wchar_t*>(lpMsgBuf);
				LocalFree(lpMsgBuf);
				return str;
			}
		};
	};
	class lookup {
	public:
		static std::wstring last_error(unsigned long dwLastError = -1) {
			if (dwLastError == -1) {
				dwLastError = GetLastError();
			}
			return ::error::format::from_system(dwLastError);
		}
		static std::string last_error_ansi(unsigned long dwLastError = -1) {
			if (dwLastError == -1) {
				dwLastError = GetLastError();
			}
			return strEx::wstring_to_string(::error::format::from_system(dwLastError));
		}
	};
}
