#pragma once

#include <string>


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
private:
	HANDLE  hServerStopEvent;
	SERVICE_STATUS          ssStatus;
	SERVICE_STATUS_HANDLE   sshStatusHandle;
	DWORD                   dwErr;
	SERVICE_TABLE_ENTRY *dispatchTable;
public:
	NTService() : dispatchTable(NULL), hServerStopEvent(NULL), dwErr(0) {
		// TODO This ought to be made dynamic somehow...
		dispatchTable = new SERVICE_TABLE_ENTRY[2];
		dispatchTable[0].lpServiceName = SZSERVICENAME;
		dispatchTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)TBase::service_main_dispatch;
		dispatchTable[1].lpServiceName = NULL;
		dispatchTable[1].lpServiceProc = NULL;
	}
	virtual ~NTService() {
		delete [] dispatchTable;
	}

	void StartServiceCtrlDispatcher() {
		::StartServiceCtrlDispatcher(dispatchTable);
	}

	void service_main(DWORD dwArgc, LPTSTR *lpszArgv)
	{
		// register our service control handler:
		sshStatusHandle = RegisterServiceCtrlHandler( TEXT(SZSERVICENAME), TBase::service_ctrl_dispatch);

		// SERVICE_STATUS members that don't change in example
		ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		ssStatus.dwServiceSpecificExitCode = 0;

		// report the status to the service control manager.
		if (!ReportStatusToSCMgr(SERVICE_START_PENDING,NO_ERROR,3000)) {
			if (sshStatusHandle)
				ReportStatusToSCMgr(SERVICE_STOPPED,dwErr,0);
		}

		ServiceStart( dwArgc, lpszArgv );

		// try to report the stopped status to the service control manager.
		if (sshStatusHandle)
			ReportStatusToSCMgr(SERVICE_STOPPED,dwErr,0);
	}


	void service_ctrl(DWORD dwCtrlCode) {
		switch(dwCtrlCode) 
		{
		case SERVICE_CONTROL_STOP:
			ReportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 0);
			ServiceStop();
			return;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;

		}
		ReportStatusToSCMgr(ssStatus.dwCurrentState, NO_ERROR, 0);
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
	BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
		static DWORD dwCheckPoint = 1;
		BOOL fResult = TRUE;

		if (dwCurrentState == SERVICE_START_PENDING)
			ssStatus.dwControlsAccepted = 0;
		else
			ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

		ssStatus.dwCurrentState = dwCurrentState;
		ssStatus.dwWin32ExitCode = dwWin32ExitCode;
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
		if (!ReportStatusToSCMgr(SERVICE_RUNNING,NO_ERROR,0)) {
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
