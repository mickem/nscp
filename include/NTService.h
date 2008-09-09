/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <string>
#include <sysinfo.h>

namespace service_helper {
	class service_exception {
		std::wstring what_;
	public:
		service_exception(std::wstring what) : what_(what) {
			OutputDebugString((std::wstring(_T("ERROR:")) + what).c_str());
		}
		std::wstring what() {
			return what_;
		}
	};
/**
 * @ingroup NSClient++
 * Helper class to implement a NT service
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 * @todo 
 *
 * @bug 
 *
 */
template <class TBase>
class NTService : public TBase
{
public:
private:
	HANDLE  hServerStopEvent;
	SERVICE_STATUS          ssStatus;
	SERVICE_STATUS_HANDLE   sshStatusHandle;
	SERVICE_TABLE_ENTRY *dispatchTable;
	DWORD dwControlsAccepted;
	std::wstring name_;
	wchar_t *serviceName_;
public:
	NTService(std::wstring name) : dispatchTable(NULL), hServerStopEvent(NULL), name_(name), dwControlsAccepted(SERVICE_ACCEPT_STOP) {
		serviceName_ = new wchar_t[name.length()+2];
		wcsncpy(serviceName_, name.c_str(), name.length());
		dispatchTable = new SERVICE_TABLE_ENTRY[2];
		dispatchTable[0].lpServiceName = serviceName_;
		dispatchTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)TBase::service_main_dispatch;
		dispatchTable[1].lpServiceName = NULL;
		dispatchTable[1].lpServiceProc = NULL;
	}
	virtual ~NTService() {
		delete [] dispatchTable;
		delete [] serviceName_;
	}

	boolean StartServiceCtrlDispatcher() {
		BOOL ret = ::StartServiceCtrlDispatcher(dispatchTable);
		if (ret == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
			OutputDebugString(_T("We are running in console mode, terminating..."));
			return false;
		}
		return ret != 0;
	}



	typedef DWORD (WINAPI *LPHANDLER_FUNCTION_EX) (
		DWORD dwControl,     // requested control code
		DWORD dwEventType,   // event type
		LPVOID lpEventData,  // event data
		LPVOID lpContext     // user-defined context data
		);
	typedef SERVICE_STATUS_HANDLE (WINAPI *REGISTER_SERVICE_CTRL_HANDLER_EX) (
		LPCTSTR lpServiceName,                // name of service
		LPHANDLER_FUNCTION_EX lpHandlerProc,  // handler function
		LPVOID lpContext                      // user data
		);

	SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerEx_(LPCTSTR lpServiceName, LPHANDLER_FUNCTION_EX lpHandlerProc, LPVOID lpContext) {
		HMODULE hAdvapi32 = NULL;
		REGISTER_SERVICE_CTRL_HANDLER_EX fRegisterServiceCtrlHandlerEx = NULL;

		if ((hAdvapi32 = GetModuleHandle(TEXT("Advapi32.dll"))) == NULL)
			return 0;
#ifdef _UNICODE
		fRegisterServiceCtrlHandlerEx = (REGISTER_SERVICE_CTRL_HANDLER_EX)GetProcAddress(hAdvapi32, "RegisterServiceCtrlHandlerExW");
#else
		fRegisterServiceCtrlHandlerEx = (REGISTER_SERVICE_CTRL_HANDLER_EX)GetProcAddress(hAdvapi32, "RegisterServiceCtrlHandlerExA");
#endif // _UNICODE
		if (!fRegisterServiceCtrlHandlerEx)
			return 0;
		return fRegisterServiceCtrlHandlerEx(lpServiceName, lpHandlerProc, lpContext);
	}



	void service_main(DWORD dwArgc, LPTSTR *lpszArgv)
	{
		OutputDebugString(_T("service_main launcing..."));
		sshStatusHandle = RegisterServiceCtrlHandlerEx_(name_.c_str(), TBase::service_ctrl_dispatch_ex, NULL);
		if (sshStatusHandle == 0) {
			OutputDebugString(_T("Failed to register RegisterServiceCtrlHandlerEx_ (attempting to use normal one)..."));
			sshStatusHandle = RegisterServiceCtrlHandler(name_.c_str(), TBase::service_ctrl_dispatch);
		} else {
			if (systemInfo::isAboveXP(systemInfo::getOSVersion()))
				dwControlsAccepted |= SERVICE_ACCEPT_SESSIONCHANGE;
			else
				OutputDebugString(_T("Not >w2k so sessiong messages disabled..."));
		}
		if (sshStatusHandle == 0)
			throw service_exception(_T("Failed to register service: ") + error::lookup::last_error());


		ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		ssStatus.dwServiceSpecificExitCode = 0;
		ssStatus.dwControlsAccepted = dwControlsAccepted;

		// report the status to the service control manager.
		if (!ReportStatusToSCMgr(SERVICE_START_PENDING,3000)) {
			ReportStatusToSCMgr(SERVICE_STOPPED,0);
			throw service_exception(_T("Failed to report service status: ") + error::lookup::last_error());
		}
		try {
			OutputDebugString(std::wstring(_T("Attempting to start service with: ") + strEx::ihextos(dwControlsAccepted)).c_str());
			ServiceStart(dwArgc, lpszArgv);
		} catch (...) {
			throw service_exception(_T("Uncaught exception in service... terminating: ") + error::lookup::last_error());
		}
		ReportStatusToSCMgr(SERVICE_STOPPED,0);
	}

	typedef struct tagWTSSESSION_NOTIFICATION
	{
		DWORD cbSize;
		DWORD dwSessionId;

	} WTSSESSION_NOTIFICATION, *PWTSSESSION_NOTIFICATION;
#define WTS_SESSION_LOGON                  0x5
#define WTS_SESSION_LOGOFF                 0x6

	DWORD service_ctrl_ex(DWORD dwCtrlCode, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) {
		switch(dwCtrlCode) 
		{
		case SERVICE_CONTROL_STOP:
			ReportStatusToSCMgr(SERVICE_STOP_PENDING, 0);
			ServiceStop();
			return 0;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		case SERVICE_CONTROL_SESSIONCHANGE:
			if (lpEventData != NULL && dwEventType == WTS_SESSION_LOGON)
				service_on_session_changed(reinterpret_cast<WTSSESSION_NOTIFICATION*>(lpEventData)->dwSessionId, true, dwEventType);
			else if (lpEventData != NULL && dwEventType == WTS_SESSION_LOGOFF)
				service_on_session_changed(reinterpret_cast<WTSSESSION_NOTIFICATION*>(lpEventData)->dwSessionId, false, dwEventType);
			else {
				service_on_session_changed(reinterpret_cast<WTSSESSION_NOTIFICATION*>(lpEventData)->dwSessionId, false, dwEventType);
			}
			break;

		default:
			break;

		}
		ReportStatusToSCMgr(ssStatus.dwCurrentState, 0);
		return 0;
	}



	/**
	* Sets the current status of the service and reports it to the Service Control Manager
	*
	* @param dwCurrentState 
	* @param dwWin32ExitCode 
	* @param dwWaitHint 
	* @return 
	*
	* @author mickem
	*
	* @date 03-13-2004
	*
	*/
	BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWaitHint) {
		static DWORD dwCheckPoint = 1;
		BOOL fResult = TRUE;

		if (dwCurrentState == SERVICE_START_PENDING)
			ssStatus.dwControlsAccepted = 0;
		else
			ssStatus.dwControlsAccepted = dwControlsAccepted;

		ssStatus.dwCurrentState = dwCurrentState;
		ssStatus.dwWin32ExitCode = 0;
		ssStatus.dwWaitHint = dwWaitHint;

		if ( (dwCurrentState == SERVICE_RUNNING ) || (dwCurrentState == SERVICE_STOPPED) )
			ssStatus.dwCheckPoint = 0;
		else
			ssStatus.dwCheckPoint = dwCheckPoint++;

		// Report the status of the service to the service control manager.
		fResult = SetServiceStatus( sshStatusHandle, &ssStatus);

		return fResult;
	}

	/**
	* Actual code of the service that does the work.
	*
	* @param dwArgc 
	* @param *lpszArgv 
	*
	* @author mickem
	*
	* @date 03-13-2004
	*
	*/
	void ServiceStart(DWORD dwArgc, LPTSTR *lpszArgv) {
		hServerStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!ReportStatusToSCMgr(SERVICE_RUNNING,0)) {
			if (hServerStopEvent)
				CloseHandle(hServerStopEvent);
			return;
		}

		TBase::InitiateService();

		WaitForSingleObject( hServerStopEvent,INFINITE );

		TBase::TerminateService();

		if (hServerStopEvent)
			CloseHandle(hServerStopEvent);
	}



	/**
	* Stops the service
	*
	*
	* @author mickem
	*
	* @date 03-13-2004
	*
	*/
	void ServiceStop() {
		if ( hServerStopEvent )
			SetEvent(hServerStopEvent);
	}
};
}