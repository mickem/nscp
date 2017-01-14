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

#pragma once


namespace remote_processes {

	class rp_exception {
		std::wstring what_;
	public:
		rp_exception(std::wstring what) : what_(what) {
#ifdef _DEBUG
			std::cout << _T("SharedMemorHandler throw an exception: ") << what << std::endl;
#endif

		}
		std::wstring what() { return what_; }
	};
/*
	typedef BOOL (WINAPI *LPFN_WTSEnumerateProcesses)(HANDLE,DWORD,DWORD,PWTS_PROCESS_INFO*,DWORD*);
	LPFN_WTSEnumerateProcesses lpfnWTSEnumerateProcesses = NULL;
	BOOL _WTSEnumerateProcesses(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFO *ppProcessInfo, DWORD *pCount) {
		if (lpfnWTSGetActiveConsoleSessionId == NULL) {
			HMODULE hMod = GetModuleHandle(_T("kernel32.dll"));
			if (hMod != NULL)
				lpfnWTSGetActiveConsoleSessionId = (LPFN_WTSGetActiveConsoleSessionId)GetProcAddress(hMod, "WTSGetActiveConsoleSessionId"); 
		}
		if (lpfnWTSGetActiveConsoleSessionId == NULL)
			return FALSE;
		lpfnWTSGetActiveConsoleSessionId(hServer, Reserved, Version, ppProcessInfo, pCount);
	}
*/

	typedef enum _WTS_CONNECTSTATE_CLASS { WTSActive, WTSConnected, WTSConnectQuery, WTSShadow, WTSDisconnected, WTSIdle, WTSListen, WTSReset, WTSDown, WTSInit, } WTS_CONNECTSTATE_CLASS;
	typedef struct _WTS_SESSION_INFO{
		DWORD SessionId;
		LPTSTR pWinStationName;
		WTS_CONNECTSTATE_CLASS State;
	} WTS_SESSION_INFO, * PWTS_SESSION_INFO;
	#define WTS_CURRENT_SERVER_HANDLE ((HANDLE)NULL)
	typedef BOOL (WINAPI *LPFN_WTSEnumerateSessions)(HANDLE,DWORD,DWORD,PWTS_SESSION_INFO*,DWORD*);
	LPFN_WTSEnumerateSessions lpfnWTSEnumerateSessions = NULL;
	BOOL _WTSEnumerateSessions(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFO *ppSessionInfo, DWORD *pCount) {
		if (lpfnWTSEnumerateSessions == NULL) {
			HMODULE hMod = LoadLibrary(_T("Wtsapi32.dll"));
			if (hMod != NULL)
				lpfnWTSEnumerateSessions = (LPFN_WTSEnumerateSessions)GetProcAddress(hMod, "WTSEnumerateSessionsW"); 
		}
		if (lpfnWTSEnumerateSessions == NULL)
			return FALSE;
		return lpfnWTSEnumerateSessions(hServer, Reserved, Version, ppSessionInfo, pCount);
	}


	typedef DWORD (WINAPI *LPFN_WTSGetActiveConsoleSessionId)(VOID);
	LPFN_WTSGetActiveConsoleSessionId lpfnWTSGetActiveConsoleSessionId = NULL;
	DWORD _WTSGetActiveConsoleSessionId() {
		if (lpfnWTSGetActiveConsoleSessionId == NULL) {
			HMODULE hMod = GetModuleHandle(_T("kernel32.dll"));
			if (hMod != NULL)
				lpfnWTSGetActiveConsoleSessionId = (LPFN_WTSGetActiveConsoleSessionId)GetProcAddress(hMod, "WTSGetActiveConsoleSessionId"); 
		}
		if (lpfnWTSGetActiveConsoleSessionId == NULL)
			return 0;
		return lpfnWTSGetActiveConsoleSessionId();
	}
	DWORD getActiveSessionId() {
		return _WTSGetActiveConsoleSessionId();
	}


	typedef BOOL (WINAPI *LPFN_WTSQueryUserToken)(ULONG SessionId, PHANDLE phToken);
	LPFN_WTSQueryUserToken lpfWTSQueryUserToken = NULL;
	BOOL _WTSQueryUserToken(ULONG SessionId, PHANDLE phToken) {
		if (lpfWTSQueryUserToken == NULL) {
			//HMODULE hmod = LoadLibrary(_T("winsta.dll"));
			HMODULE hmod = LoadLibrary(_T("Wtsapi32.dll"));
			if (hmod == NULL)
				return FALSE;
			lpfWTSQueryUserToken = (LPFN_WTSQueryUserToken)GetProcAddress(hmod, "WTSQueryUserToken");
		}
		if (lpfWTSQueryUserToken == NULL)
			return FALSE;
		return lpfWTSQueryUserToken(SessionId, phToken);
	}

	BOOL GetSessionUserToken(ULONG SessionId, LPHANDLE lphUserToken) {
		BOOL   bResult = FALSE;
		HANDLE hImpersonationToken = INVALID_HANDLE_VALUE;
		if (lphUserToken != NULL) {
			if (_WTSQueryUserToken (SessionId, &hImpersonationToken)) {
				bResult = DuplicateTokenEx(hImpersonationToken, 0, NULL, SecurityImpersonation, TokenPrimary, lphUserToken);
				CloseHandle(hImpersonationToken);
			}            
		}
		return bResult;
	}

}