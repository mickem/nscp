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
#include "StdAfx.h"
#include ".\wmiquery.h"

#include <objidl.h>
#include <Wbemidl.h>
#include <map>

WMIQuery::WMIQuery(void) : bInitialized(false)
{
}

WMIQuery::~WMIQuery(void)
{
	if (bInitialized)
		unInitialize();
}


bool WMIQuery::initialize()
{
	NSC_LOG_ERROR_STD("Attempting Inialized WMI component");

	HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hRes)) {
		NSC_LOG_ERROR_STD("CoInitialize failed: " + error::format::from_system(hRes));
		return false;
	}
	bInitialized = true;
	hRes = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_PKT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);
	if (FAILED(hRes)) {
		NSC_LOG_ERROR_STD("CoInitializeSecurity failed: " + error::format::from_system(hRes));
		return false;
	}

	NSC_LOG_ERROR_STD("Inialized WMI component");
	return true;
}
void WMIQuery::unInitialize()
{
	CoUninitialize();
	bInitialized = false;
}

std::string WMIQuery::sanitize_string(LPTSTR in) {
	TCHAR *p = in;
	while (*p) {
		if (p[0] < ' ' || p[0] > '}')
			p[0] = '.';
		p++;
	} 
	return in;
}

WMIQuery::result_type WMIQuery::execute(std::string query)
{
	if (!bInitialized) {
		initialize();
	}
	result_type ret;

	CComPtr< IWbemLocator > locator;
	HRESULT hr = CoCreateInstance( CLSID_WbemAdministrativeLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast< void** >( &locator ) );
	if (FAILED(hr)) {
		throw WMIException("CoCreateInstance for CLSID_WbemAdministrativeLocator failed!", hr);
	}

	BSTR bstrNamespace = (L"root\\cimv2");
	CComPtr< IWbemServices > service;
	hr = locator->ConnectServer( bstrNamespace, NULL, NULL, NULL, WBEM_FLAG_CONNECT_USE_MAX_WAIT, NULL, NULL, &service );
	if (FAILED(hr)) {
		throw WMIException("ConnectServer failed!", hr);
	}
	CComBSTR strQuery(query.c_str());
	BSTR strQL = (L"WQL");

	CComPtr< IEnumWbemClassObject > enumerator;
	hr = service->ExecQuery( strQL, strQuery, WBEM_FLAG_FORWARD_ONLY, NULL, &enumerator );
	if (FAILED(hr)) {
		throw WMIException("ExecQuery failed:" + query + " (reason is: " + ComError::getComError() + ")", hr);
	}

	CComPtr< IWbemClassObject > row = NULL;
	ULONG retcnt;
	int i=0;
	while (hr = enumerator->Next( WBEM_INFINITE, 1L, &row, &retcnt ) == WBEM_S_NO_ERROR) {
		if (SUCCEEDED(hr)) {
			if (retcnt > 0) {
				SAFEARRAY* pstrNames;
				wmi_row returnRow;
				hr = row->GetNames(NULL,WBEM_FLAG_ALWAYS|WBEM_FLAG_NONSYSTEM_ONLY,NULL,&pstrNames);
				if (FAILED(hr)) {
					throw WMIException("GetNames failed:" + query, hr);
				}

				long index = 0, begin, end;
				CComSafeArray<BSTR> arr = pstrNames;
				begin = arr.GetLowerBound();
				end = arr.GetUpperBound();
				for ( index = begin; index <= end; index++ ) {
					USES_CONVERSION;
					CComBSTR bColumn = arr.GetAt(index);
					std::string column = OLE2T(bColumn);
					CComVariant vValue;
					hr = row->Get(bColumn, 0, &vValue, 0, 0);
					if (FAILED(hr)) {
						throw WMIException("Failed to get value for " + column + " in query: " + query, hr);
					}
					WMIResult value;

					if (vValue.vt == VT_INT) {
						value.setNumeric(column, vValue.intVal);
					} else if (vValue.vt == VT_I4) {
						value.setNumeric(column, vValue.lVal);
					} else if (vValue.vt == VT_UINT) {
						value.setNumeric(column, vValue.uintVal);
					} else if (vValue.vt == VT_BSTR) {
						value.setString(column, OLE2T(vValue.bstrVal));
					} else if (vValue.vt == VT_NULL) {
						value.setString(column, "NULL");
					} else if (vValue.vt == VT_BOOL) {
						value.setBoth(column, vValue.iVal, vValue.iVal?"TRUE":"FALSE");
					} else {
						NSC_LOG_ERROR_STD(column + " is not supported (type-id: " + strEx::itos(vValue.vt) + ")");
					}
					returnRow.addValue(column, value);
				}
				ret.push_back(returnRow);
			}
		}
		row.Release();
	}
	return ret;
}
