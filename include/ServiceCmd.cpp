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
#include <error.hpp>

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
	void Install(std::wstring szName, std::wstring szDisplayName, std::wstring szDependencies, DWORD dwServiceType, std::wstring args, std::wstring exe) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;

		if (exe.empty()) {
			TCHAR szPath[512];
			if ( GetModuleFileName( NULL, szPath, 512 ) == 0 )
				throw SCException(_T("Could not get module"));
			exe = szPath;
		}

		std::wstring bin = _T("\"") + exe + _T("\"");
		if (!args.empty())
			bin += _T(" ") + args;
		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed:") + error::lookup::last_error());
		schService = CreateService(
			schSCManager,               // SCManager database
			szName.c_str(),				// name of service
			szDisplayName.c_str(),		// name to display
			SERVICE_ALL_ACCESS,         // desired access
			dwServiceType,				// service type
			SERVICE_AUTO_START,			// start type
			SERVICE_ERROR_NORMAL,       // error control type
			bin.c_str(),                // service's binary
			NULL,                       // no load ordering group
			NULL,                       // no tag identifier
			szDependencies.c_str(),	    // dependencies
			NULL,                       // LocalSystem account
			NULL);                      // no password

		if (!schService) {
			DWORD err = GetLastError();
			CloseServiceHandle(schSCManager);
			if (err==ERROR_SERVICE_EXISTS) {
				throw SCException(_T("Service already installed!"));
			}
			throw SCException(_T("Unable to install service.") + error::lookup::last_error(err));
		}
		std::wcout << _T("Service ") << szName << _T(" (") << bin << _T(") installed...") << std::endl;;
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
			throw SCException(_T("OpenSCManager failed: ") + error::lookup::last_error());
		schService = OpenService(schSCManager, szName, SERVICE_ALL_ACCESS);
		if (!schService) {
			DWORD err = GetLastError();
			CloseServiceHandle(schSCManager);
			throw SCException(_T("Unable to open service: ") + error::lookup::last_error(err));
		}
		BOOL result = ChangeServiceConfig(schService, dwServiceType, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE , NULL, NULL, NULL, 
			NULL, NULL, NULL, NULL);
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		if (result != TRUE)
			throw SCException(_T("Could not change service information"));
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
			throw SCException(_T("Unable to open service: ") + error::lookup::last_error(err));
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
			throw SCException(_T("OpenSCManager failed: ") + error::lookup::last_error());
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
					throw SCException(_T("Service '") +name + _T("' failed to start."));
				}
			}
			CloseServiceHandle(schService);
		} else {
			std::wstring err = _T("OpenService on '") + name + _T("' failed: ") + error::lookup::last_error();
			CloseServiceHandle(schSCManager);
			throw SCException(err);
		}
		CloseServiceHandle(schSCManager);
	}
	bool isInstalled(std::wstring name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager) 
			throw SCException(_T("OpenSCManager failed: ") + error::lookup::last_error());
		schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (schService) {
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return true;
		} else {
			CloseServiceHandle(schSCManager);
			return false;
		}
	}
	std::wstring get_exe_path(std::wstring svc_name) {
		std::wstring ret;
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager) 
			throw SCException(_T("OpenSCManager failed: ") + error::lookup::last_error());
		schService = OpenService(schSCManager, svc_name.c_str(),  SERVICE_QUERY_CONFIG);
		if (!schService) {
			CloseServiceHandle(schSCManager);
			throw SCException(_T("Failed to open service: ") + svc_name + _T(" because ") + error::lookup::last_error());
		}
		DWORD dwBytesNeeded=0;
		DWORD lErr;
		if (QueryServiceConfig(schService,NULL,0, &dwBytesNeeded) || ((lErr = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)) {
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			throw SCException(_T("Failed to query service information: ") + svc_name + _T(" because ") + error::lookup::last_error(lErr));
		}

		LPQUERY_SERVICE_CONFIG lpqscBuf = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, dwBytesNeeded+10); 
		BOOL bRet = (lpqscBuf!=NULL) && (QueryServiceConfig(schService,lpqscBuf,dwBytesNeeded, &dwBytesNeeded)==TRUE);
		if (!bRet)
			lErr = GetLastError();
		else {
			ret = lpqscBuf->lpBinaryPathName;
		}
		LocalFree(lpqscBuf);
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		if (!bRet)
			throw SCException(_T("Failed to query service information: ") + svc_name + _T(" because ") + error::lookup::last_error(lErr));
		return ret;
	}

	bool isStarted(std::wstring name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		SERVICE_STATUS ssStatus;
		bool ret = false;

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS );
		if (!schSCManager) 
			throw SCException(_T("OpenSCManager failed: ") + error::lookup::last_error());
		schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (schService) {
			if ( QueryServiceStatus( schService, &ssStatus ) ) {
				if ( ssStatus.dwCurrentState == SERVICE_RUNNING ) {
					ret = true;
				} else if ( ssStatus.dwCurrentState == SERVICE_START_PENDING ) {
					ret = true;
				}
			}
			CloseServiceHandle(schService);
		} else {
			CloseServiceHandle(schSCManager);
			ret = false;
		}
		CloseServiceHandle(schSCManager);
		return ret;
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
			throw SCException(_T("OpenSCManager failed: ") + error::lookup::last_error());
		schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (schService) {
			if(!DeleteService(schService)) {
				std::wstring err = _T("DeleteService failed: ") + error::lookup::last_error();
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				throw SCException(err);
			}
			CloseServiceHandle(schService);
		} else {
			std::wstring err = _T("OpenService failed: ") + error::lookup::last_error();
			CloseServiceHandle(schSCManager);
			throw SCException(err);
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
			throw SCException(_T("OpenSCManager failed: ") + error::lookup::last_error());
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
			std::wstring err = _T("OpenService failed: ") + error::lookup::last_error();
			CloseServiceHandle(schSCManager);
			throw SCException(err);
		}
		CloseServiceHandle(schSCManager);
	}

	void StopNoWait(std::wstring name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		SERVICE_STATUS ssStatus;

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException(_T("OpenSCManager failed."));
		schService = OpenService(schSCManager, name.c_str(), SERVICE_ALL_ACCESS);
		if (schService) {
			// try to stop the service
			ControlService( schService, SERVICE_CONTROL_STOP, &ssStatus );
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
