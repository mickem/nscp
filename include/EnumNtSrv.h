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

//=============================================================================
// class TNtServiceInfo
//
//=============================================================================

class NTServiceException {
private:
	std::wstring name_;
	std::wstring msg_;
	unsigned int error_;
public:
	NTServiceException(std::wstring name,std::wstring msg,unsigned int error) : name_(name), error_(error), msg_(msg) {};

	std::wstring getError() {
		return _T("Service: ") + name_ + _T(" caused: ") + msg_ + _T("(") + strEx::itos(error_) + _T(")");
	}
};

class TNtServiceInfo {
public:
	std::wstring m_strServiceName;
	std::wstring m_strDisplayName;
	std::wstring m_strBinaryPath;
	DWORD m_dwServiceType;
	DWORD m_dwStartType;
	DWORD m_dwErrorControl;
	DWORD m_dwCurrentState;

public:
	TNtServiceInfo();
	TNtServiceInfo(const TNtServiceInfo& source);
	TNtServiceInfo& operator=(const TNtServiceInfo& source);
	virtual ~TNtServiceInfo();

	std::wstring GetServiceType(void);
	std::wstring GetStartType(void);
	std::wstring GetErrorControl(void);
	std::wstring GetCurrentState(void);

	static TNtServiceInfoList EnumServices(DWORD dwType, DWORD dwState);
	//static void EnumServices(DWORD dwType, DWORD dwState, TNtServiceInfoList *pList);
	static TNtServiceInfo GetService(std::wstring);
};

#endif

/*#############################################################################
# End of file ENUMNTSRV.H
#############################################################################*/
