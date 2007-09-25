#pragma once

#include <string>
#include <windows.h>

namespace error {
	class format {
	public:
		static std::string from_system(unsigned long dwError) {
			LPVOID lpMsgBuf;
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
			if (dwRet == 0) {
				return "failed to lookup error code: " + strEx::itos(dwError);
			}
			TCHAR *szBuf = new TCHAR[dwRet + 100];
			wsprintf(szBuf, "%d: %s", dwError, lpMsgBuf); 
			std::string str = szBuf;
			LocalFree(lpMsgBuf);
			return str;
		}
		static std::string from_module(std::string module, unsigned long dwError) {
			LPVOID lpMsgBuf;
			unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_HMODULE,GetModuleHandle(module.c_str()),dwError,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&lpMsgBuf,0,NULL);
			if (dwRet == 0) {
				return "failed to lookup error code: " + strEx::itos(dwError);
			}
			TCHAR *szBuf = new TCHAR[dwRet + 100];
			wsprintf(szBuf, "%d: %s", dwError, lpMsgBuf); 
			std::string str = szBuf;
			LocalFree(lpMsgBuf);
			return str;
		}
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
