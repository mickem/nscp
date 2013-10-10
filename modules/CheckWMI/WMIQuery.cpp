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

#include <error_com.hpp>

namespace wmi_impl {
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

	void set_proxy_blanket(IUnknown *pProxy, const std::string &user, const std::string &password) {
		if (user.empty() || password.empty())
			return;
		identidy_container auth = get_identity(utf8::cvt<std::wstring>(user), utf8::cvt<std::wstring>(password));
		HRESULT hr = CoSetProxyBlanket(pProxy, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_DEFAULT, 
			RPC_C_IMP_LEVEL_IMPERSONATE, &auth.auth_identity, EOAC_NONE );
		if (FAILED(hr))
			throw wmi_exception("CoSetProxyBlanket failed: " + ComError::getComError(ComError::getWMIError(hr)));
	}

	template <class T, enum VARENUM U>
	std::wstring parse_item(T value);

	template<>
	std::wstring parse_item<SHORT,VT_BOOL>(SHORT value) {
		return value?_T("TRUE"):_T("FALSE");
	}
	template<>
	std::wstring parse_item<BSTR,VT_BSTR>(BSTR value) {
		return OLE2T(value);
	}
	template<>
	std::wstring parse_item<UINT,VT_UI1>(UINT value) {
		return strEx::itos(value);
	}
	template<>
	std::wstring parse_item<LONG,VT_I4>(LONG value) {
		return strEx::itos(value);
	}
	template<class T, enum VARENUM U>
	std::wstring array_to_string(std::string tag, CComVariant &vValue) {
		SAFEARRAY* paArray = vValue.parray;

		T * array = NULL;

		SafeArrayAccessData(paArray, (void**)&array);

		long lLBound = 0;
		long lUBound = 0;
		long nCount = 0;

		if (FAILED(SafeArrayGetLBound(paArray, 1, &lLBound)) ||
			FAILED(SafeArrayGetUBound(paArray, 1, &lUBound)))
		{
			NSC_LOG_ERROR("Failed to get bounds for array: " + tag);
			return _T("UNKNOWN");
		}
		nCount = ( lUBound - lLBound + 1 );
		std::wstring result;
		for (int i=0; i < nCount; i++) {
			std::wstring s = parse_item<T,U>(array[i]);
			//strEx::append_list(result, s);
		}
		SafeArrayUnaccessData(paArray);
		return result;
	}

	WMIQuery::WMIResult get_value(std::string tag, CComVariant &vValue) {
		WMIQuery::WMIResult value;
		if (vValue.vt == VT_INT) {
			value.setNumeric(vValue.intVal);
		}else if (vValue.vt == VT_I2) {
			value.setNumeric(vValue.iVal);
		} else if (vValue.vt == VT_I4) {
			value.setNumeric(vValue.lVal);
		} else if (vValue.vt == VT_R4) {
			value.setNumeric(vValue.fltVal);
		} else if (vValue.vt == VT_R8) {
			value.setNumeric(vValue.dblVal);
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
		} else if (vValue.vt == (VT_ARRAY|VT_BSTR)) {
			value.setString(array_to_string<BSTR,VT_BSTR>(tag, vValue));
		} else if (vValue.vt == (VT_ARRAY|VT_BOOL)) {
			value.setString(array_to_string<SHORT,VT_BOOL>(tag, vValue));
		} else if (vValue.vt == (VT_ARRAY|VT_UI1)) {
			value.setString(array_to_string<UINT,VT_UI1>(tag, vValue));
		} else if (vValue.vt == (VT_ARRAY|VT_I4)) {
			value.setString(array_to_string<LONG,VT_I4>(tag, vValue));
		} else {
			value.setString(_T("UNKNOWN"));
			NSC_LOG_ERROR(tag + " is not supported (type-id: " + strEx::s::xtos(vValue.vt) + ")");
		}
		return value;
	}

	WMIQuery::result_type WMIQuery::get_classes(std::wstring ns, std::wstring superClass, std::wstring user, std::wstring password) {

		result_type ret;
// 		CComPtr<IWbemServices> service = create_service(ns, user, password);
// 
// 		CComBSTR strSuperClass = superClass.c_str();
// 
// 		CComPtr<IEnumWbemClassObject> enumerator;
// 		HRESULT hr = service->CreateClassEnum(strSuperClass,WBEM_FLAG_DEEP| WBEM_FLAG_USE_AMENDED_QUALIFIERS|
// 			WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY, NULL, &enumerator);
// 		if (FAILED(hr))
// 			throw WMIException("CreateClassEnum failed: " + ComError::getComError(ComError::getWMIError(hr)) + ")");
// 
// 		CComPtr<IWbemClassObject> row = NULL;
// 		ULONG retcnt;
// 		while (hr = enumerator->Next( WBEM_INFINITE, 1L, &row, &retcnt ) == WBEM_S_NO_ERROR) {
// 			if (SUCCEEDED(hr) && (retcnt > 0)) {
// 				wmi_row returnRow;
// 				WMIResult value;
// 				CComBSTR classProp = _T("__CLASS");
// 				CComVariant v;
// 				hr = row->Get(classProp, 0, &v, 0, 0);
// 
// 				if (SUCCEEDED(hr) && (v.vt == VT_BSTR))	{O
// 					value.setString(std::wstring(OLE2T(v.bstrVal)));
// 					returnRow.addValue(_T("__CLASS"), value);
// 				} else {
// 					value.setString(_T("Unknown"));
// 					returnRow.addValue(_T("__CLASS"), value);
// 				}
// 				ret.push_back(returnRow);
// 			}
// 			row.Release();
// 		}
		return ret;
	}

	WMIQuery::result_type WMIQuery::get_instances(std::wstring ns, std::wstring superClass, std::wstring user, std::wstring password) {
		result_type ret;
// 		CComPtr<IWbemServices> service = create_service(ns, user, password);
// 
// 		CComBSTR strSuperClass = superClass.c_str();
// 		CComPtr<IEnumWbemClassObject> enumerator;
// 		HRESULT hr = service->CreateInstanceEnum(strSuperClass,WBEM_FLAG_SHALLOW|WBEM_FLAG_USE_AMENDED_QUALIFIERS|WBEM_FLAG_RETURN_IMMEDIATELY|WBEM_FLAG_FORWARD_ONLY, 
// 			NULL, &enumerator);
// 		if (FAILED(hr))
// 			throw WMIException("CreateInstanceEnum failed: " + ComError::getComError(ComError::getWMIError(hr)) + ")");
// 
// 
// 		set_proxy_blanket(enumerator, user, password);
// 		ULONG retcnt;
// 		CComPtr< IWbemClassObject > row = NULL;
// 		hr = enumerator->Next(WBEM_INFINITE, 1L, &row, &retcnt);
// 		if (FAILED(hr))
// 			throw WMIException("Enumeration of failed: " + ComError::getComError(ComError::getWMIError(hr)) + ")");
// 
// 		while (hr == WBEM_S_NO_ERROR) {
// 			if (retcnt > 0) {
// 				wmi_row returnRow;
// 				WMIResult value;
// 				CComBSTR classProp = _T("Name");
// 				CComVariant v;
// 				hr = row->Get(classProp, 0, &v, 0, 0);
// 
// 				if (SUCCEEDED(hr) && (v.vt == VT_BSTR))	{
// 					value.setString(std::wstring(OLE2T(v.bstrVal)));
// 					returnRow.addValue(_T("Name"), value);
// 				} else {
// 					value.setString(_T("Unknown"));
// 					returnRow.addValue(_T("Name"), value);
// 				}
// 
// 
// 				SAFEARRAY* pstrNames;
// 				hr = row->GetNames(NULL,WBEM_FLAG_ALWAYS|WBEM_FLAG_NONSYSTEM_ONLY,NULL,&pstrNames);
// 				if (FAILED(hr))
// 					throw WMIException("GetNames failed:", hr);
// 
// 				long begin, end;
// 				CComSafeArray<BSTR> arr = pstrNames;
// 				begin = arr.GetLowerBound();
// 				end = arr.GetUpperBound();
// 				for (long index = begin; index <= end; index++ ) {
// 					try {
// 						CComBSTR bColumn = arr.GetAt(index);
// 						std::wstring column = bColumn.m_str;
// 						CComVariant vValue;
// 						hr = row->Get(bColumn, 0, &vValue, 0, 0);
// 						if (FAILED(hr))
// 							throw WMIException("Failed to get value for " + utf8::cvt<std::string>(column) + " in query: ", hr);
// 						returnRow.addValue(column, get_value(column, vValue));
// 					} catch (const std::exception &e) {
// 						throw WMIException("Failed to convert data: " + utf8::utf8_from_native(e.what()));
// 					}
// 				}
// 
// 				ret.push_back(returnRow);
// 			}
// 			row.Release();
// 			hr = enumerator->Next(WBEM_INFINITE, 1L, &row, &retcnt);
// 			if (FAILED(hr))
// 				throw wmi_exception("Enumeration of failed: " + ComError::getComError(ComError::getWMIError(hr)) + ")");
// 		}
		return ret;
	}

	void query::init_query() {

		CComPtr<IWbemLocator> locator;
		HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast< void** >(&locator));
		if (FAILED(hr))
			throw wmi_exception("CoCreateInstance for CLSID_WbemAdministrativeLocator failed!", hr);

		CComBSTR strNamespace(utf8::cvt<std::wstring>(ns).c_str());
		CComBSTR strUser(utf8::cvt<std::wstring>(username).c_str());
		CComBSTR strPassword(utf8::cvt<std::wstring>(password).c_str());

		// @todo: WBEM_FLAG_CONNECT_USE_MAX_WAIT
		hr = locator->ConnectServer(strNamespace, username.empty()?NULL:strUser, password.empty()?NULL:strPassword, NULL, NULL, 0, NULL, &service );
		if (FAILED(hr))
			throw wmi_exception("ConnectServer failed: namespace=" + ns + ", user=" + username, hr);

		set_proxy_blanket(service, username, password);

	}
	std::list<std::string> query::get_columns() {
		init_query();
		std::list<std::string> ret;
		BSTR strQL = _T("WQL");
		CComBSTR strQuery(utf8::cvt<std::wstring>(wql_query).c_str());

		CComPtr<IEnumWbemClassObject> enumerator;
		HRESULT hr = service->ExecQuery(strQL, strQuery, WBEM_FLAG_PROTOTYPE, NULL, &enumerator);
		if (FAILED(hr))
			throw wmi_exception("ExecQuery of '" + wql_query + "' failed: " + ComError::getComError(ComError::getWMIError(hr)));


		ULONG retcnt;
		CComPtr<IWbemClassObject> row;
		hr = enumerator->Next(WBEM_INFINITE, 1L, &row, &retcnt);
		if (FAILED(hr) || !row)
			throw wmi_exception("Enumeration of '" + wql_query + "' failed: " + ComError::getComError(ComError::getWMIError(hr)));
		if (retcnt == 0) 
			throw wmi_exception("Query returned no rows: " + wql_query);


		SAFEARRAY* pstrNames;
		hr = row->GetNames(NULL,WBEM_FLAG_ALWAYS|WBEM_FLAG_NONSYSTEM_ONLY,NULL,&pstrNames);
		if (FAILED(hr))
			throw wmi_exception("GetNames failed:" + wql_query, hr);

		long begin, end;
		CComSafeArray<BSTR> arr = pstrNames;
		begin = arr.GetLowerBound();
		end = arr.GetUpperBound();
		for (long index = begin; index <= end; index++ ) {
			CComBSTR bColumn = arr.GetAt(index);
			std::string column = utf8::cvt<std::string>(bColumn.m_str);
			ret.push_back(column);
		}
		return ret;

	}

	std::string row::get_string(const std::string col) {
		CComBSTR bCol(utf8::cvt<std::wstring>(col).c_str());
		CComVariant vValue;
		HRESULT hr = row_obj->Get(bCol, 0, &vValue, 0, 0);
		if (FAILED(hr))
			throw wmi_exception("Failed to get value for " + col + ": ", hr);
		WMIQuery::WMIResult r = get_value(col, vValue);
		return r.get_string();
	}

	long long row::get_int(const std::string col) {
		CComBSTR bCol(utf8::cvt<std::wstring>(col).c_str());
		CComVariant vValue;
		HRESULT hr = row_obj->Get(bCol, 0, &vValue, 0, 0);
		if (FAILED(hr))
			throw wmi_exception("Failed to get value for " + col + ": ", hr);
		WMIQuery::WMIResult r = get_value(col, vValue);
		return r.numeric;
	}

	bool row_enumerator::has_next() {
		ULONG retcnt;
		if (row_instance.row_obj)
			row_instance.row_obj.Release();
		HRESULT hr = enumerator_obj->Next(WBEM_INFINITE, 1L, &row_instance.row_obj, &retcnt);
		if (FAILED(hr))
			throw wmi_exception("Enumeration failed: " + ComError::getComError(ComError::getWMIError(hr)));
		return hr == WBEM_S_NO_ERROR;
	}


	row& row_enumerator::get_next() {
		return row_instance;
	}
	row_enumerator query::execute() {
		row_enumerator ret;
		BSTR strQL = _T("WQL");
		CComBSTR strQuery(utf8::cvt<std::wstring>(wql_query).c_str());

		HRESULT hr = service->ExecQuery(strQL, strQuery, WBEM_FLAG_FORWARD_ONLY, NULL, &ret.enumerator_obj);
		if (FAILED(hr))
			throw wmi_exception("ExecQuery of '" + wql_query + "' failed: " + ComError::getComError(ComError::getWMIError(hr)));
		return ret;
	}

	std::string ComError::getComError(std::wstring inDesc /*= _T("")*/)
	{
		return error::com::get();
	}
}