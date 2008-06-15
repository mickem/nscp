/*#############################################################################
# ENUMNTSRV.CPP
#
# SCA Software International S.A.
# http://www.scasoftware.com
# scaadmin@scasoftware.com
#
# Copyright (c) 1999 SCA Software International S.A.
#
# Date: 05.12.1999.
# Author: Zoran M.Todorovic
#
# This software is provided "AS IS", without a warranty of any kind.
# You are free to use/modify this code but leave this header intact.
#
#############################################################################*/

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <WinSvc.h>
#include "EnumNtSrv.h"

#ifdef _DEBUG
//#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define ASSERT(x)

//=============================================================================
// class TNtServiceInfo
//
//=============================================================================

TNtServiceInfo::TNtServiceInfo()
{
	m_strServiceName.clear();
	m_strDisplayName.clear();
	m_strBinaryPath.clear();
	m_dwServiceType = 0;
	m_dwStartType = 0;
	m_dwErrorControl = 0;
	m_dwCurrentState = 0;
}

TNtServiceInfo::TNtServiceInfo(const TNtServiceInfo& source)
{
	*this = source;
}

TNtServiceInfo::~TNtServiceInfo()
{
}

TNtServiceInfo& TNtServiceInfo::operator=(const TNtServiceInfo& source)
{
	m_strServiceName = source.m_strServiceName;
	m_strDisplayName = source.m_strDisplayName;
	m_strBinaryPath = source.m_strBinaryPath;
	m_dwServiceType = source.m_dwServiceType;
	m_dwStartType = source.m_dwStartType;
	m_dwErrorControl = source.m_dwErrorControl;
	m_dwCurrentState = source.m_dwCurrentState;
	return *this;
}

// Return a service type as a string
std::wstring TNtServiceInfo::GetServiceType(void)
{
	std::wstring str = _T("UNKNOWN");
	if (m_dwServiceType & SERVICE_WIN32) {
		if (m_dwServiceType & SERVICE_WIN32_OWN_PROCESS)
			str = _T("WIN32_OWN_PROCESS");
		else if (m_dwServiceType & SERVICE_WIN32_SHARE_PROCESS)
			str = _T("WIN32_SHARE_PROCESS");
		if (m_dwServiceType & SERVICE_INTERACTIVE_PROCESS)
			str += _T(" (INTERACTIVE_PROCESS)");
	}
	switch (m_dwServiceType) {
	case SERVICE_KERNEL_DRIVER: 
		str = _T("KERNEL_DRIVER"); 
		break;
	case SERVICE_FILE_SYSTEM_DRIVER: 
		str = _T("FILE_SYSTEM_DRIVER"); 
		break;
	};
	return str;
}

// Return a service start type as a string
std::wstring TNtServiceInfo::GetStartType(void)
{
	TCHAR *types[] = {
		_T("BOOT_START"),		// 0
			_T("SYSTEM_START"), // 1
			_T("AUTO_START"),	// 2
			_T("DEMAND_START"), // 3
			_T("DISABLED")		// 4
	};
	return std::wstring(types[m_dwStartType]);
}

// Return this service error control as a string
std::wstring TNtServiceInfo::GetErrorControl(void)
{
	ASSERT(m_dwErrorControl < 4);
	TCHAR *types[] = {
		_T("ERROR_IGNORE"),		// 0
			_T("ERROR_NORMAL"), // 1
			_T("ERROR_SEVERE"), // 2
			_T("ERROR_CRITICAL")// 3
	};
	return std::wstring(types[m_dwErrorControl]);
}

// Return this service current state as a string
std::wstring TNtServiceInfo::GetCurrentState(void)
{
	ASSERT(m_dwCurrentState < 8);
	TCHAR *types[] = {
		_T("UNKNOWN"),
			_T("STOPPED"),			// 1
			_T("START_PENDING"),	// 2
			_T("STOP_PENDING"),		// 3
			_T("RUNNING"),			// 4
			_T("CONTINUE_PENDING"), // 5
			_T("PAUSE_PENDING"),	// 6
			_T("PAUSED")			// 7
	};
	return std::wstring(types[m_dwCurrentState]);
}

// Enumerate services on this machine and return a pointer to an array of objects.
// Caller is responsible to delete this pointer using delete [] ...
// dwType = bit OR of SERVICE_WIN32, SERVICE_DRIVER
// dwState = bit OR of SERVICE_ACTIVE, SERVICE_INACTIVE
TNtServiceInfoList TNtServiceInfo::EnumServices(DWORD dwType, DWORD dwState)
{
	TNtServiceInfoList ret;
	// Maybe check if dwType and dwState have at least one constant specified
	SC_HANDLE scman = ::OpenSCManager(NULL,NULL,SC_MANAGER_ENUMERATE_SERVICE);
	if (scman) {
		ENUM_SERVICE_STATUS service, *lpservice;
		BOOL rc;
		DWORD bytesNeeded,servicesReturned,resumeHandle = 0;
		rc = ::EnumServicesStatus(scman,dwType,dwState,&service,sizeof(service),
			&bytesNeeded,&servicesReturned,&resumeHandle);
		if ((rc == FALSE) && (::GetLastError() == ERROR_MORE_DATA)) {
			DWORD bytes = bytesNeeded + sizeof(ENUM_SERVICE_STATUS);
			lpservice = new ENUM_SERVICE_STATUS [bytes];
			::EnumServicesStatus(scman,dwType,dwState,lpservice,bytes,
				&bytesNeeded,&servicesReturned,&resumeHandle);
			TCHAR Buffer[1024];				// Should be enough for service info
			QUERY_SERVICE_CONFIG *lpqch = (QUERY_SERVICE_CONFIG*)Buffer;
			for (DWORD ndx = 0; ndx < servicesReturned; ndx++) {
				TNtServiceInfo info;
				info.m_strServiceName = lpservice[ndx].lpServiceName;
				info.m_strDisplayName = lpservice[ndx].lpDisplayName;
				info.m_dwServiceType = lpservice[ndx].ServiceStatus.dwServiceType;
				info.m_dwCurrentState = lpservice[ndx].ServiceStatus.dwCurrentState;
				SC_HANDLE sh = ::OpenService(scman,lpservice[ndx].lpServiceName,SERVICE_QUERY_CONFIG);
				if (::QueryServiceConfig(sh,lpqch,sizeof(Buffer),&bytesNeeded)) {
					info.m_strBinaryPath = lpqch->lpBinaryPathName;
					info.m_dwStartType = lpqch->dwStartType;
					info.m_dwErrorControl = lpqch->dwErrorControl;
				}
				::CloseServiceHandle(sh);
				ret.push_back(info);
			}
			delete [] lpservice;
		}
		::CloseServiceHandle(scman);
	}
	return ret;
}


#define SC_BUF_LEN 4096
TNtServiceInfo TNtServiceInfo::GetService(std::wstring name)
{
	TNtServiceInfo info;
	info.m_strServiceName = name;
	SC_HANDLE scman = ::OpenSCManager(NULL,NULL,SC_MANAGER_ENUMERATE_SERVICE);
	if (!scman) {
		throw NTServiceException(name, _T("Could not open ServiceControl manager"), GetLastError());
	}
	SC_HANDLE sh = ::OpenService(scman,name.c_str(),SERVICE_QUERY_STATUS);
	if (!sh) {
		DWORD bufLen = SC_BUF_LEN;
		TCHAR *buf = new TCHAR[bufLen+1];
		if (GetServiceKeyName(scman, name.c_str(), buf, &bufLen) == 0) {
			::CloseServiceHandle(scman);
			delete [] buf;
			throw NTServiceException(name, _T("GetServiceKeyName: Could not translate service name"), GetLastError());
		}
		/*
		Why does this not work? (a bug in the API? says it should return the correct size?)
		if (bufLen >= SC_BUF_LEN) {
			::CloseServiceHandle(scman);
			throw NTServiceException(name, "Service name to long to handle", GetLastError());
		}
		buf[bufLen] = 0;
		*/
		sh = ::OpenService(scman,buf,SERVICE_QUERY_STATUS);
		delete [] buf;
		if (sh == NULL) {
			::CloseServiceHandle(scman);
			throw NTServiceException(name, _T("OpenService: Could not open Service"), GetLastError());
		}
	}
	SERVICE_STATUS state;
	if (::QueryServiceStatus(sh, &state)) {
		info.m_dwCurrentState = state.dwCurrentState;
		info.m_dwServiceType = state.dwServiceType;
	} else {
		::CloseServiceHandle(sh);
		::CloseServiceHandle(scman);
		throw NTServiceException(name, _T("QueryServiceStatus: Could not query service status"), GetLastError());
	}
	// TODO: Get more info here 
	::CloseServiceHandle(sh);
	::CloseServiceHandle(scman);
	return info;
}

/*
// Enumerate services on this machine and return an STL list of service objects 
// dwType = bit OR of SERVICE_WIN32, SERVICE_DRIVER
// dwState = bit OR of SERVICE_ACTIVE, SERVICE_INACTIVE
void TNtServiceInfo::EnumServices(DWORD dwType, DWORD dwState, TNtServiceInfoList *pList)
{
	ASSERT(pList != NULL);
	TNtServiceInfo *pSrvList = NULL;
	DWORD dwCount = 0;
	pSrvList = TNtServiceInfo::EnumServices(dwType, dwState, &dwCount);
	for (DWORD dwIndex = 0; dwIndex < dwCount; dwIndex ++) {
		pList->insert(pList->end(), pSrvList[dwIndex]);
	}
	delete [] pSrvList;
}
*/
/*#############################################################################
# End of file ENUMNTSRV.CPP
#############################################################################*/
