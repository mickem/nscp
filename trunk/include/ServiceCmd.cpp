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

#include "stdafx.h"
#include "ServiceCmd.h"

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
	void Install(LPCTSTR szName, LPCTSTR szDisplayName, LPCTSTR szDependencies) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		TCHAR szPath[512];

		if ( GetModuleFileName( NULL, szPath, 512 ) == 0 )
			throw SCException("Could not get module");

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException("OpenSCManager failed.");
		schService = CreateService(
			schSCManager,               // SCManager database
			TEXT(szName),				// name of service
			TEXT(szDisplayName),		 // name to display
			SERVICE_ALL_ACCESS,         // desired access
			SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS ,  // service type
			SERVICE_DEMAND_START,       // start type
			SERVICE_ERROR_NORMAL,       // error control type
			szPath,                     // service's binary
			NULL,                       // no load ordering group
			NULL,                       // no tag identifier
			TEXT(szDependencies),       // dependencies
			NULL,                       // LocalSystem account
			NULL);                      // no password

		if (!schService) {
			CloseServiceHandle(schSCManager);
			throw SCException("Unable to install service.");
		}
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
	}

	/**
	 * Stars the service.
	 *
	 * @param szName 
	 *
	 * @author mickem
	 *
	 * @date 03-13-2004
	 *
	 */
	void Start(std::string name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		SERVICE_STATUS ssStatus;

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS );
		if (!schSCManager) 
			throw SCException("OpenSCManager failed.");
		schService = OpenService(schSCManager, TEXT(name.c_str()), SERVICE_ALL_ACCESS);
		if (schService) {
			// try to stop the service
			if ( StartService(schService,0,NULL) ) {
				std::cout << "Starting " << name;
				Sleep( 1000 );
				while( QueryServiceStatus( schService, &ssStatus ) ) {
					if ( ssStatus.dwCurrentState == SERVICE_START_PENDING ) {
						std::cout << ".";
						Sleep( 1000 );
					} else
						break;
				}
				if ( ssStatus.dwCurrentState != SERVICE_RUNNING ) {
					CloseServiceHandle(schService);
					CloseServiceHandle(schSCManager);
					throw SCException("Service failed to start.");
				}
			}
			CloseServiceHandle(schService);
		} else {
			CloseServiceHandle(schSCManager);
			throw SCException("OpenService failed.");
		}
		CloseServiceHandle(schSCManager);
	}

	/**
	 * Stops and removes the service
	 *
	 * @param szName 
	 *
	 * @author mickem
	 *
	 * @date 03-13-2004
	 *
	 */
	void Uninstall(std::string name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		Stop(name);

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException("OpenSCManager failed.");
		schService = OpenService(schSCManager, TEXT(name.c_str()), SERVICE_ALL_ACCESS);
		if (schService) {
			if(!DeleteService(schService)) {
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				throw SCException("DeleteService failed.");
			}
			CloseServiceHandle(schService);
		} else {
			CloseServiceHandle(schSCManager);
			throw SCException("OpenService failed.");
		}
		CloseServiceHandle(schSCManager);
	}

	/**
	 * Stops the service
	 *
	 * @param szName 
	 *
	 * @author MickeM
	 *
	 * @date 03-13-2004
	 *
	 */
	void Stop(std::string name) {
		SC_HANDLE   schService;
		SC_HANDLE   schSCManager;
		SERVICE_STATUS ssStatus;

		schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		if (!schSCManager)
			throw SCException("OpenSCManager failed.");
		schService = OpenService(schSCManager, TEXT(name.c_str()), SERVICE_ALL_ACCESS);
		if (schService) {
			// try to stop the service
			if ( ControlService( schService, SERVICE_CONTROL_STOP, &ssStatus ) ) {
				std::cout << "Stopping service.";
				Sleep( 1000 );
				while( QueryServiceStatus( schService, &ssStatus ) ) {
					if ( ssStatus.dwCurrentState == SERVICE_STOP_PENDING ) {
						std::cout << ".";
						Sleep( 1000 );
					} else
						break;
				}
				std::cout << std::endl;
				if ( ssStatus.dwCurrentState != SERVICE_STOPPED ) {
					CloseServiceHandle(schService);
					CloseServiceHandle(schSCManager);
					throw SCException("Service failed to stop.");
				}
			}
			CloseServiceHandle(schService);
		} else {
			CloseServiceHandle(schSCManager);
			throw SCException("OpenService failed.");
		}
		CloseServiceHandle(schSCManager);
	}
}