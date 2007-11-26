//////////////////////////////////////////////////////////////////////////
// Service Helpers.
// 
// Functions to stop/start/install and uninstall a service.
//
// Copyright (c) 2004 MySolutions NORDIC (http://www.medin.nu)
//
// Date: 2004-03-13
// Author: Michael Medin (mickem@medin.nu)
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tchar.h>
#include "ServiceCmd.h"
#include <strEx.h>
#include <tchar.h>
#include <iostream>
#include <msvc_wrappers.h>

namespace serviceControll {
	/**
	 * Installs the service
	 *
	 * @param szName 
	 * @param szDisplayName 
	 * @param szDependencies 
	 *
	 * @author mickem
	 *
	 * @date 03-13-2004
	 *
	 */
	void Install(LPCTSTR szName, LPCTSTR szDisplayName, LPCTSTR szDependencies, DWORD dwServiceType) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		TCHAR szPath[512];

		if ( GetModuleFileName( NULL, szPath, 512 ) == 0 )
			throw SCException(_T("Could not get module"));

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed."));
		schService = CreateService(
			schSCManager,               // SCManager database
			szName,						// name of service
			szDisplayName,				// name to display
			SERVICE_ALL_ACCESS,         // desired access
			dwServiceType,				// service type
			SERVICE_AUTO_START,			// start type
			SERVICE_ERROR_NORMAL,       // error control type
			szPath,                     // service's binary
			NULL,                       // no load ordering group
			NULL,                       // no tag identifier
			szDependencies,			    // dependencies
			NULL,                       // LocalSystem account
			NULL);                      // no password

		if (!schService) {
			DWORD err = GetLastError();
			CloseServiceHandle(schSCManager);
			if (err==ERROR_SERVICE_EXISTS) {
				throw SCException(_T("Service already installed!"));
			}
			throw SCException(_T("Unable to install service."), err);
		}
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
	}

	void ModifyServiceType(LPCTSTR szName, DWORD dwServiceType) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		TCHAR szPath[512];

		if ( GetModuleFileName( NULL, szPath, 512 ) == 0 )
			throw SCException(_T("Could not get module"));

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed."));
		schService = OpenService(schSCManager, szName, SERVICE_ALL_ACCESS);
		if (!schService) {
			DWORD err = GetLastError();
			CloseServiceHandle(schSCManager);
			throw SCException(_T("Unable to open service."), err);
		}
		BOOL result = ChangeServiceConfig(schService, dwServiceType, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE , NULL, NULL, NULL, 
			NULL, NULL, NULL, NULL);
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		if (result != TRUE)
			throw SCException(_T("Could not query service information"));
	}

	DWORD GetServiceType(LPCTSTR szName) {
		LPQUERY_SERVICE_CONFIG lpqscBuf = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LPTR, 4096); 
		if (lpqscBuf == NULL) {
			throw SCException(_T("Could not allocate memory"));
		}
		SC_HANDLE schService;
		SC_HANDLE schSCManager;
		TCHAR szPath[512];

		if ( GetModuleFileName( NULL, szPath, 512 ) == 0 )
			throw SCException(_T("Could not get module"));

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed."));
		schService = OpenService(schSCManager, szName, SERVICE_ALL_ACCESS);
		if (!schService) {
			DWORD err = GetLastError();
			CloseServiceHandle(schSCManager);
			throw SCException(_T("Unable to open service."), err);
		}


		DWORD dwBytesNeeded = 0;
		BOOL success = QueryServiceConfig(schService, lpqscBuf,  4096, &dwBytesNeeded);
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		if (success != TRUE)
			throw SCException(_T("Could not query service information"));
		DWORD ret = lpqscBuf->dwServiceType;
		LocalFree(lpqscBuf);
		return ret;
	}

	/**
	 * Stars the service.
	 *
	 * @param name The name of the service to start 
	 *
	 * @author mickem
	 *
	 * @date 03-13-2004
	 *
	 */
	void Start(std::wstring name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		SERVICE_STATUS ssStatus;

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS );
		if (!schSCManager) 
			throw SCException(_T("OpenSCManager failed."));
		schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (schService) {
			// try to stop the service
			if ( StartService(schService,0,NULL) ) {
				std::wcout << _T("Starting ") << name;
				Sleep( 1000 );
				while( QueryServiceStatus( schService, &ssStatus ) ) {
					if ( ssStatus.dwCurrentState == SERVICE_START_PENDING ) {
						std::wcout << _T(".");
						Sleep( 1000 );
					} else
						break;
				}
				if ( ssStatus.dwCurrentState != SERVICE_RUNNING ) {
					CloseServiceHandle(schService);
					CloseServiceHandle(schSCManager);
					throw SCException(_T("Service failed to start."));
				}
			}
			CloseServiceHandle(schService);
		} else {
			CloseServiceHandle(schSCManager);
			throw SCException(_T("OpenService failed."));
		}
		CloseServiceHandle(schSCManager);
	}

	/**
	 * Stops and removes the service
	 *
	 * @param name The name of the service to uninstall 
	 *
	 * @author mickem
	 *
	 * @date 03-13-2004
	 *
	 */
	void Uninstall(std::wstring name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		Stop(name);

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed."));
		schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (schService) {
			if(!DeleteService(schService)) {
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				throw SCException(_T("DeleteService failed."));
			}
			CloseServiceHandle(schService);
		} else {
			CloseServiceHandle(schSCManager);
			throw SCException(_T("OpenService failed."));
		}
		CloseServiceHandle(schSCManager);
	}

	/**
	 * Stops the service
	 *
	 * @param name The name of the serive to stop 
	 *
	 * @author MickeM
	 *
	 * @date 03-13-2004
	 *
	 */
	void Stop(std::wstring name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		SERVICE_STATUS ssStatus;

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed."));
		schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (schService) {
			// try to stop the service
			if ( ControlService( schService, SERVICE_CONTROL_STOP, &ssStatus ) ) {
				std::wcout << _T("Stopping service.");
				Sleep( 1000 );
				while( QueryServiceStatus( schService, &ssStatus ) ) {
					if ( ssStatus.dwCurrentState == SERVICE_STOP_PENDING ) {
						std::wcout << _T(".");
						Sleep( 1000 );
					} else
						break;
				}
				std::wcout << std::endl;
				if ( ssStatus.dwCurrentState != SERVICE_STOPPED ) {
					CloseServiceHandle(schService);
					CloseServiceHandle(schSCManager);
					throw SCException(_T("Service failed to stop."));
				}
			}
			CloseServiceHandle(schService);
		} else {
			CloseServiceHandle(schSCManager);
			throw SCException(_T("OpenService failed."));
		}
		CloseServiceHandle(schSCManager);
	}


	typedef BOOL (WINAPI*PFChangeServiceConfig2)(SC_HANDLE hService,DWORD dwInfoLevel,LPVOID lpInfo);

	void SetDescription(std::wstring name, std::wstring desc) {
		PFChangeServiceConfig2 FChangeServiceConfig2;
		HMODULE ADVAPI= ::LoadLibrary(_T("Advapi32"));
		if (!ADVAPI) {
			throw SCException(_T("Couldn't set extended service info (ignore this on NT4)."));
		}
#ifdef UNICODE
		FChangeServiceConfig2 = (PFChangeServiceConfig2)::GetProcAddress(ADVAPI, "ChangeServiceConfig2W");
#else
		FChangeServiceConfig2 = (PFChangeServiceConfig2)::GetProcAddress(ADVAPI, _TEXT("ChangeServiceConfig2A"));
#endif
		if (!FChangeServiceConfig2) {
			FreeLibrary(ADVAPI);
			throw SCException(_T("Couldn't set extended service info (ignore this on NT4)."));
		}
		SERVICE_DESCRIPTION descr;
		SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed."));
		SC_HANDLE schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (!schService) {
			FreeLibrary(ADVAPI);
			CloseServiceHandle(schSCManager);
			throw SCException(_T("OpenService failed."));
		}

		TCHAR* d = new TCHAR[desc.length()+2];
		wcsncpy_s(d, desc.length()+2, desc.c_str(), desc.length());
		descr.lpDescription = d;
		BOOL bResult = FChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &descr);
		delete [] d;
		FreeLibrary(ADVAPI);
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		if (!bResult)
			throw SCException(_T("ChangeServiceConfig2 failed."));
	}
}
