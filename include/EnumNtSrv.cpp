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
#include <error.hpp>
#include "EnumNtSrv.h"

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
std::string TNtServiceInfo::GetServiceType(void)
{
	std::string str = "UNKNOWN";
	if (m_dwServiceType & SERVICE_WIN32) {
		if (m_dwServiceType & SERVICE_WIN32_OWN_PROCESS)
			str = "WIN32_OWN_PROCESS";
		else if (m_dwServiceType & SERVICE_WIN32_SHARE_PROCESS)
			str = "WIN32_SHARE_PROCESS";
		if (m_dwServiceType & SERVICE_INTERACTIVE_PROCESS)
			str += " (INTERACTIVE_PROCESS)";
	}
	switch (m_dwServiceType) {
	case SERVICE_KERNEL_DRIVER: 
		str = "KERNEL_DRIVER";
		break;
	case SERVICE_FILE_SYSTEM_DRIVER: 
		str = "FILE_SYSTEM_DRIVER";
		break;
	};
	return str;
}

// Return a service start type as a string
std::string TNtServiceInfo::GetStartType(void)
{
	char *types[] = {
		"BOOT_START",		// 0
			"SYSTEM_START", // 1
			"AUTO_START",	// 2
			"DEMAND_START", // 3
			"DISABLED",		// 4
			"DELAYED"		// 5
	};
	return types[m_dwStartType];
}

// Return this service error control as a string
std::string TNtServiceInfo::GetErrorControl(void)
{
	if (m_dwErrorControl >= 4)
		throw std::exception();
	char *types[] = {
		"ERROR_IGNORE",		// 0
			"ERROR_NORMAL", // 1
			"ERROR_SEVERE", // 2
			"ERROR_CRITICAL"// 3
	};
	return types[m_dwErrorControl];
}

// Return this service current state as a string
std::string TNtServiceInfo::GetCurrentState(void)
{
	if (m_dwErrorControl >= 8)
		throw std::exception();
	char *types[] = {
		"UNKNOWN",
			"STOPPED",			// 1
			"START_PENDING",	// 2
			"STOP_PENDING",		// 3
			"RUNNING",			// 4
			"CONTINUE_PENDING", // 5
			"PAUSE_PENDING",	// 6
			"PAUSED"			// 7
	};
	return types[m_dwCurrentState];
}

#ifndef SERVICE_CONFIG_DELAYED_AUTO_START_INFO
#define SERVICE_CONFIG_DELAYED_AUTO_START_INFO 3
#endif

typedef struct NSCP_SERVICE_DELAYED_AUTO_START_INFO {
	BOOL fDelayedAutostart;
};

template<class T>
struct return_data {
	TCHAR *buffer;
	DWORD buffer_size;
	T *object;

	return_data() : buffer(NULL), object(NULL), buffer_size(0) {}
	return_data(const return_data &other)  : buffer(NULL), object(NULL), buffer_size(other.buffer_size){
		if (other.buffer_size > 0) {
			buffer = new TCHAR[other.buffer_size];
			memcpy(buffer, other.buffer, other.buffer_size);
			object = reinterpret_cast<T*>(buffer);
		}
	}
	return_data* operator= (const return_data &other) {
		if (other.buffer_size > 0) {
			buffer = new TCHAR[other.buffer_size];
			memcpy(buffer, other.buffer, other.buffer_size);
			buffer_size = other.buffer_size;
			object = reinterpret_cast<T*>(buffer);
		}
		return *this;
	}
	~return_data() {
		if (buffer)
			delete [] buffer;
	}
	void resize() {
		if (buffer != NULL)
			delete [] buffer;
		buffer_size += 10;
		buffer = new TCHAR[buffer_size];
		object = reinterpret_cast<T*>(buffer);
	}
	void clean() {
		if (buffer != NULL)
			delete [] buffer;
		buffer = NULL;
		object = NULL;
		buffer_size = NULL;
	}
	bool has_data() const {
		return buffer != NULL && object != NULL && buffer_size > 0;
	}
};
template<class T>
return_data<T> NSCP_QueryServiceConfig2(SC_HANDLE hService, DWORD dwInfoLevel) {
	return_data<T> ret;
	if (!QueryServiceConfig2(hService, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, NULL, 0, &ret.buffer_size) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
		ret.resize();
		if (QueryServiceConfig2(hService, dwInfoLevel, reinterpret_cast<LPBYTE>(&ret.buffer), ret.buffer_size, &ret.buffer_size)) {
			return ret;
		}
	}
	ret.clean();
	return ret;
}

// Enumerate services on this machine and return a pointer to an array of objects.
// Caller is responsible to delete this pointer using delete [] ...
// dwType = bit OR of SERVICE_WIN32, SERVICE_DRIVER
// dwState = bit OR of SERVICE_ACTIVE, SERVICE_INACTIVE
TNtServiceInfoList TNtServiceInfo::EnumServices(DWORD dwType, DWORD dwState, bool vista)
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
				info.m_strServiceName = utf8::cvt<std::string>(lpservice[ndx].lpServiceName);
				info.m_strDisplayName = utf8::cvt<std::string>(lpservice[ndx].lpDisplayName);
				info.m_dwServiceType = lpservice[ndx].ServiceStatus.dwServiceType;
				info.m_dwCurrentState = lpservice[ndx].ServiceStatus.dwCurrentState;
				SC_HANDLE hService = ::OpenService(scman,lpservice[ndx].lpServiceName, SERVICE_QUERY_CONFIG);
				if (::QueryServiceConfig(hService,lpqch,sizeof(Buffer),&bytesNeeded)) {
					info.m_strBinaryPath = utf8::cvt<std::string>(lpqch->lpBinaryPathName);
					info.m_dwStartType = lpqch->dwStartType;
					info.m_dwErrorControl = lpqch->dwErrorControl;
				}
				if (info.m_dwStartType == SERVICE_AUTO_START && vista) {
					return_data<NSCP_SERVICE_DELAYED_AUTO_START_INFO> delayed = NSCP_QueryServiceConfig2<NSCP_SERVICE_DELAYED_AUTO_START_INFO>(hService, SERVICE_CONFIG_DELAYED_AUTO_START_INFO);
					if (delayed.has_data() && delayed.object->fDelayedAutostart)
						info.m_dwStartType = NSCP_SERVICE_DELAYED;
				}
				::CloseServiceHandle(hService);
				ret.push_back(info);
			}
			delete [] lpservice;
		}
		::CloseServiceHandle(scman);
	}
	return ret;
}


#define SC_BUF_LEN 4096
TNtServiceInfo TNtServiceInfo::GetService(std::string name)
{
	TNtServiceInfo info;
	info.m_strServiceName = name;
	SC_HANDLE scman = ::OpenSCManager(NULL,NULL,SC_MANAGER_ENUMERATE_SERVICE);
	if (!scman) {
		throw NTServiceException(name, "Could not open ServiceControl manager: " + error::lookup::last_error());
	}
	SC_HANDLE sh = ::OpenService(scman, utf8::cvt<std::wstring>(name).c_str(),SERVICE_QUERY_STATUS);
	if (!sh) {
		std::string short_name;
		DWORD bufLen = SC_BUF_LEN;
		TCHAR *buf = new TCHAR[bufLen+1];
		if (GetServiceKeyName(scman, utf8::cvt<std::wstring>(name).c_str(), buf, &bufLen) == 0) {
			short_name = name;
		} else {
			short_name = utf8::cvt<std::string>(buf);
		}
		delete [] buf;
		sh = ::OpenService(scman,utf8::cvt<std::wstring>(short_name).c_str(),SERVICE_QUERY_STATUS);
		if (sh == NULL) {
			DWORD dwErr = GetLastError();
			::CloseServiceHandle(scman);
			if (dwErr == ERROR_SERVICE_DOES_NOT_EXIST) {
				info.m_dwCurrentState = MY_SERVICE_NOT_FOUND;
				info.m_dwServiceType = MY_SERVICE_NOT_FOUND;
				return info;
			} else {
				throw NTServiceException(name, "OpenService: Could not open Service: " + error::lookup::last_error(dwErr));
			}
		}
	}
	SERVICE_STATUS state;
	if (::QueryServiceStatus(sh, &state)) {
		info.m_dwCurrentState = state.dwCurrentState;
		info.m_dwServiceType = state.dwServiceType;
	} else {
		::CloseServiceHandle(sh);
		::CloseServiceHandle(scman);
		throw NTServiceException(name, "QueryServiceStatus: Could not query service status: " + error::lookup::last_error());
	}
	// TODO: Get more info here 
	::CloseServiceHandle(sh);
	::CloseServiceHandle(scman);
	return info;
}

/*#############################################################################
# End of file ENUMNTSRV.CPP
#############################################################################*/
