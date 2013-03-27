/*#############################################################################
# ENUMNTSRV.H
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

#ifndef __ENUMNTSRV_H__
#define __ENUMNTSRV_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <list>
#include <string>
#include <strEx.h>

class TNtServiceInfo;

typedef std::list<TNtServiceInfo> TNtServiceInfoList;

#define NSCP_SERVICE_DELAYED 5
//=============================================================================
// class TNtServiceInfo
//
//=============================================================================

class NTServiceException : public std::exception {
private:
	std::string name_;
	std::string msg_;
public:
	NTServiceException(std::string name,std::string msg) : name_(name), msg_(msg) {};
	~NTServiceException() throw() {}

	const char* what() const throw() {
		return std::string("Service: '" + name_ + "' caused: " + msg_).c_str();
	}
};
#define MY_SERVICE_NOT_FOUND                        0xffff0000

class TNtServiceInfo {
public:
	std::string m_strServiceName;
	std::string m_strDisplayName;
	std::string m_strBinaryPath;
	DWORD m_dwServiceType;
	DWORD m_dwStartType;
	DWORD m_dwErrorControl;
	DWORD m_dwCurrentState;

public:
	TNtServiceInfo();
	TNtServiceInfo(const TNtServiceInfo& source);
	TNtServiceInfo& operator=(const TNtServiceInfo& source);
	virtual ~TNtServiceInfo();

	std::string GetServiceType();
	std::string GetStartType();
	std::string GetErrorControl();
	std::string GetCurrentState();

	static TNtServiceInfoList EnumServices(DWORD dwType, DWORD dwState, bool vista);
	//static void EnumServices(DWORD dwType, DWORD dwState, TNtServiceInfoList *pList);
	static TNtServiceInfo GetService(std::string);
};

#endif

/*#############################################################################
# End of file ENUMNTSRV.H
#############################################################################*/
