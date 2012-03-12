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

#include <map>

#include <boost/optional.hpp>

#include <objidl.h>
#include <Wbemidl.h>
#include <WMIUtils.h>


std::wstring WMIQuery::sanitize_string(LPTSTR in) {
	wchar_t *p = in;
	while (*p) {
		if (p[0] < ' ' || p[0] > '}')
			p[0] = '.';
		p++;
	} 
	return in;
}

struct identidy_container {
	identidy_container(std::wstring domain, std::wstring username, std::wstring password) 
		: domain(domain)
		, username(username)
		, password(password)
	{
		setup();
	}
	identidy_container(const identidy_container &other) 
		: domain(other.domain)
		, username(other.username)
		, password(other.password)
	{
		setup();
	}
	const identidy_container& operator=(const identidy_container &other) {
		domain = other.domain;
		username = other.username;
		password = other.password;
		setup();
		return *this;
	}

	COAUTHIDENTITY auth_identity;
	std::wstring domain;
	std::wstring username;
	std::wstring password;

private:
	void setup() {
		memset(&auth_identity, 0, sizeof(COAUTHIDENTITY));
		auth_identity.PasswordLength = password.size();
		auth_identity.Password = (USHORT*)password.c_str();
		auth_identity.UserLength = username.size();
		auth_identity.User = (USHORT*)username.c_str();
		auth_identity.DomainLength = domain.size();
		auth_identity.Domain = (USHORT*)domain.c_str();
		auth_identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
	}

};

identidy_container get_identity(const std::wstring &username, const std::wstring &password) {
	std::wstring::size_type pos = username.find('\\');
	if(pos == std::string::npos) {
		return identidy_container(_T(""), username, password);
	}
	return identidy_container(username.substr(0, pos), username.substr(pos+1), password);
}

void set_proxy_blanket(IUnknown *pProxy, const std::wstring &user, const std::wstring &password) {
	if (user.empty() || password.empty())
		return;
	identidy_container auth = get_identity(user, password);
	HRESULT hr = CoSetProxyBlanket(pProxy, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_DEFAULT, 
		RPC_C_IMP_LEVEL_IMPERSONATE, &auth.auth_identity, EOAC_NONE );
	if (FAILED(hr))
		throw WMIException(_T("CoSetProxyBlanket failed: ") + ComError::getComError(ComError::getWMIError(hr)));
}


CComPtr<IWbemServices> create_service(std::wstring ns, std::wstring user, std::wstring password) {
	CComPtr<IWbemLocator> locator;
	HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast< void** >(&locator));
	if (FAILED(hr))
		throw WMIException(_T("CoCreateInstance for CLSID_WbemAdministrativeLocator failed!"), hr);

	CComBSTR strNamespace(ns.c_str());
	CComBSTR strUser(user.c_str());
	CComBSTR strPassword(password.c_str());

	CComPtr< IWbemServices > service;
	// @todo: WBEM_FLAG_CONNECT_USE_MAX_WAIT
	hr = locator->ConnectServer(strNamespace, user.empty()?NULL:strUser, password.empty()?NULL:strPassword, NULL, NULL, 0, NULL, &service );
	if (FAILED(hr))
		throw WMIException(_T("ConnectServer failed: namespace=") + ns + _T(", user=") + user, hr);

	set_proxy_blanket(service, user, password);

	return service;
}


WMIQuery::WMIResult get_value(std::wstring tag, CComVariant &vValue) {
	WMIQuery::WMIResult value;
	if (vValue.vt == VT_INT) {
		value.setNumeric(vValue.intVal);
	}else if (vValue.vt == VT_I2) {
		value.setNumeric(vValue.iVal);
	} else if (vValue.vt == VT_I4) {
		value.setNumeric(vValue.lVal);
	} else if (vValue.vt == VT_UI1) {
		value.setNumeric(vValue.uintVal);
	} else if (vValue.vt == VT_UINT) {
		value.setNumeric(vValue.uintVal);
	} else if (vValue.vt == VT_BSTR) {
		value.setString(OLE2T(vValue.bstrVal));
	} else if (vValue.vt == VT_NULL) {
		value.setString(_T("NULL"));
	} else if (vValue.vt == VT_BOOL) {
		value.setBoth(vValue.iVal, vValue.iVal?_T("TRUE"):_T("FALSE"));
	} else {
		value.setString(_T("UNKNOWN"));
		NSC_LOG_ERROR_STD(tag + _T(" is not supported (type-id: ") + strEx::itos(vValue.vt) + _T(")"));
	}
	return value;
}

WMIQuery::result_type WMIQuery::get_classes(std::wstring ns, std::wstring superClass, std::wstring user, std::wstring password) {

	result_type ret;
	CComPtr<IWbemServices> service = create_service(ns, user, password);

	CComBSTR strSuperClass = superClass.c_str();

	CComPtr<IEnumWbemClassObject> enumerator;
	HRESULT hr = service->CreateClassEnum(strSuperClass,WBEM_FLAG_DEEP| WBEM_FLAG_USE_AMENDED_QUALIFIERS|
		WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY, NULL, &enumerator);
	if (FAILED(hr))
		throw WMIException(_T("CreateClassEnum failed: ") + ComError::getComError(ComError::getWMIError(hr)) + _T(")"));

	CComPtr<IWbemClassObject> row = NULL;
	ULONG retcnt;
	while (hr = enumerator->Next( WBEM_INFINITE, 1L, &row, &retcnt ) == WBEM_S_NO_ERROR) {
		if (SUCCEEDED(hr) && (retcnt > 0)) {
			wmi_row returnRow;
			WMIResult value;
			CComBSTR classProp = _T("__CLASS");
			CComVariant v;
			hr = row->Get(classProp, 0, &v, 0, 0);

			if (SUCCEEDED(hr) && (v.vt == VT_BSTR))	{
				value.setString(std::wstring(OLE2T(v.bstrVal)));
				returnRow.addValue(_T("__CLASS"), value);
			} else {
				value.setString(_T("Unknown"));
				returnRow.addValue(_T("__CLASS"), value);
			}
			ret.push_back(returnRow);
		}
		row.Release();
	}
	return ret;
}

WMIQuery::result_type WMIQuery::get_instances(std::wstring ns, std::wstring superClass, std::wstring user, std::wstring password) {
	result_type ret;
	CComPtr<IWbemServices> service = create_service(ns, user, password);

	CComBSTR strSuperClass = superClass.c_str();
	CComPtr<IEnumWbemClassObject> enumerator;
	HRESULT hr = service->CreateInstanceEnum(strSuperClass,WBEM_FLAG_SHALLOW|WBEM_FLAG_USE_AMENDED_QUALIFIERS|WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY, 
		NULL, &enumerator);
	if (FAILED(hr))
		throw WMIException(_T("CreateInstanceEnum failed: ") + ComError::getComError(ComError::getWMIError(hr)) + _T(")"));


	set_proxy_blanket(enumerator, user, password);
	ULONG retcnt;
	CComPtr< IWbemClassObject > row = NULL;
	hr = enumerator->Next(WBEM_INFINITE, 1L, &row, &retcnt);
	if (FAILED(hr))
		throw WMIException(_T("Enumeration of failed: ") + ComError::getComError(ComError::getWMIError(hr)) + _T(")"));

	while (hr == WBEM_S_NO_ERROR) {
		if (retcnt > 0) {
			wmi_row returnRow;
			WMIResult value;
			CComBSTR classProp = _T("Name");
			CComVariant v;
			hr = row->Get(classProp, 0, &v, 0, 0);

			if (SUCCEEDED(hr) && (v.vt == VT_BSTR))	{
				value.setString(std::wstring(OLE2T(v.bstrVal)));
				returnRow.addValue(_T("Name"), value);
			} else {
				value.setString(_T("Unknown"));
				returnRow.addValue(_T("Name"), value);
			}


			SAFEARRAY* pstrNames;
			hr = row->GetNames(NULL,WBEM_FLAG_ALWAYS|WBEM_FLAG_NONSYSTEM_ONLY,NULL,&pstrNames);
			if (FAILED(hr))
				throw WMIException(_T("GetNames failed:"), hr);

			long begin, end;
			CComSafeArray<BSTR> arr = pstrNames;
			begin = arr.GetLowerBound();
			end = arr.GetUpperBound();
			for (long index = begin; index <= end; index++ ) {
				try {
					CComBSTR bColumn = arr.GetAt(index);
					std::wstring column = bColumn.m_str;
					CComVariant vValue;
					hr = row->Get(bColumn, 0, &vValue, 0, 0);
					if (FAILED(hr))
						throw WMIException(_T("Failed to get value for ") + column + _T(" in query: "), hr);
					returnRow.addValue(column, get_value(column, vValue));
				} catch (const std::exception &e) {
					throw WMIException(_T("Failed to convert data: ") + utf8::cvt<std::wstring>(e.what()));
				}
			}

			ret.push_back(returnRow);
		}
		row.Release();
		hr = enumerator->Next(WBEM_INFINITE, 1L, &row, &retcnt);
		if (FAILED(hr))
			throw WMIException(_T("Enumeration of failed: ") + ComError::getComError(ComError::getWMIError(hr)) + _T(")"));
	}
	return ret;
}

WMIQuery::result_type WMIQuery::execute(std::wstring ns, std::wstring query, std::wstring user, std::wstring password)
{
	result_type ret;
	CComPtr<IWbemServices> service = create_service(ns, user, password);

	CComBSTR strQuery(query.c_str());
	BSTR strQL = _T("WQL");

	CComPtr<IEnumWbemClassObject> enumerator;
	HRESULT hr = service->ExecQuery( strQL, strQuery, WBEM_FLAG_FORWARD_ONLY, NULL, &enumerator );
	if (FAILED(hr))
		throw WMIException(_T("ExecQuery of '") + query + _T("' failed: ") + ComError::getComError(ComError::getWMIError(hr)) + _T(")"));


	set_proxy_blanket(enumerator, user, password);
	ULONG retcnt;
	CComPtr< IWbemClassObject > row = NULL;
	hr = enumerator->Next(WBEM_INFINITE, 1L, &row, &retcnt);
	if (FAILED(hr))
		throw WMIException(_T("Enumeration of '") + query + _T("' failed: ") + ComError::getComError(ComError::getWMIError(hr)) + _T(")"));
	while (hr == WBEM_S_NO_ERROR) {
		if (retcnt > 0) {
			SAFEARRAY* pstrNames;
			wmi_row returnRow;
			hr = row->GetNames(NULL,WBEM_FLAG_ALWAYS|WBEM_FLAG_NONSYSTEM_ONLY,NULL,&pstrNames);
			if (FAILED(hr))
				throw WMIException(_T("GetNames failed:") + query, hr);

			long begin, end;
			CComSafeArray<BSTR> arr = pstrNames;
			begin = arr.GetLowerBound();
			end = arr.GetUpperBound();
			for (long index = begin; index <= end; index++ ) {
				try {
					CComBSTR bColumn = arr.GetAt(index);
					std::wstring column = bColumn.m_str;
					CComVariant vValue;
					hr = row->Get(bColumn, 0, &vValue, 0, 0);
					if (FAILED(hr))
						throw WMIException(_T("Failed to get value for ") + column + _T(" in query: ") + query, hr);
					returnRow.addValue(column, get_value(column, vValue));
				} catch (const std::exception &e) {
					throw WMIException(_T("Failed to convert data: ") + utf8::cvt<std::wstring>(e.what()));
				}
			}
			ret.push_back(returnRow);
		}
		row.Release();
		hr = enumerator->Next(WBEM_INFINITE, 1L, &row, &retcnt);
		if (FAILED(hr))
			throw WMIException(_T("Enumeration of '") + query + _T("' failed: ") + ComError::getComError(ComError::getWMIError(hr)) + _T(")"));
	}
	return ret;
}
