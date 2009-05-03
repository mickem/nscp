#pragma once
#include <tchar.h>
#include <string>
#include <windows.h>
#include <strEx.h>

namespace error {
	class format {
	public:
		static std::wstring from_system(unsigned long dwError) {
			LPVOID lpMsgBuf = NULL;
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
			if (dwRet == 0) {
				return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
			}
			TCHAR *szBuf = new TCHAR[dwRet + 100];
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
			TCHAR *szBuf = new TCHAR[dwRet + 100];
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
			TCHAR *szBuf = new TCHAR[dwRet + 100];
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
				std::wstring str = reinterpret_cast<TCHAR*>(lpMsgBuf);
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}
			static std::wstring from_module(std::wstring module, unsigned long dwError, DWORD *arguments) {
				HMODULE hDLL = LoadLibraryEx(module.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					return _T("failed to load: ") + module + _T("( reson: ") + strEx::itos(GetLastError()) + _T(")");
				}
				LPVOID lpMsgBuf;
				unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY,hDLL,
					dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,reinterpret_cast<va_list*>(arguments));
				if (dwRet == 0) {
					FreeLibrary(hDLL);
					DWORD err = GetLastError();
					if (err == 317) {
						return _T("");
					}
					return _T("failed to lookup error code: ") + strEx::itos(dwError) + _T(" from DLL: ") + module + _T("( reson: ") + strEx::itos(err) + _T(")");
				}
				std::wstring str = reinterpret_cast<TCHAR*>(lpMsgBuf);
				LocalFree(lpMsgBuf);
				FreeLibrary(hDLL);
				return str;
			}
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
				std::wstring str = reinterpret_cast<TCHAR*>(lpMsgBuf);
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
