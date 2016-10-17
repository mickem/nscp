/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <wmi/wmi_query.hpp>

#include <map>

#include <boost/optional.hpp>

#include <objidl.h>
#include <Wbemidl.h>
#include <WMIUtils.h>

#include <error_com.hpp>

namespace wmi_impl {
	struct identidy_container {
		identidy_container(std::wstring domain, std::wstring username, std::wstring password)
			: domain(domain)
			, username(username)
			, password(password) {
			setup();
		}
		identidy_container(const identidy_container &other)
			: domain(other.domain)
			, username(other.username)
			, password(other.password) {
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
			auth_identity.PasswordLength = static_cast<ULONG>(password.size());
			auth_identity.Password = const_cast<USHORT*>(reinterpret_cast<const USHORT*>(password.c_str()));
			auth_identity.UserLength = static_cast<ULONG>(username.size());
			auth_identity.User = const_cast<USHORT*>(reinterpret_cast<const USHORT*>(username.c_str()));
			auth_identity.DomainLength = static_cast<ULONG>(domain.size());
			auth_identity.Domain = const_cast<USHORT*>(reinterpret_cast<const USHORT*>(domain.c_str()));
			auth_identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
		}
	};

	identidy_container get_identity(const std::wstring &username, const std::wstring &password) {
		std::wstring::size_type pos = username.find('\\');
		if (pos == std::string::npos) {
			return identidy_container(_T(""), username, password);
		}
		return identidy_container(username.substr(0, pos), username.substr(pos + 1), password);
	}

	void set_proxy_blanket(IUnknown *pProxy, const std::string &user, const std::string &password) {
		if (user.empty() || password.empty())
			return;
		identidy_container auth = get_identity(utf8::cvt<std::wstring>(user), utf8::cvt<std::wstring>(password));
		HRESULT hr = CoSetProxyBlanket(pProxy, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_DEFAULT,
			RPC_C_IMP_LEVEL_IMPERSONATE, &auth.auth_identity, EOAC_NONE);
		if (FAILED(hr))
			throw wmi_exception(hr, "CoSetProxyBlanket failed: " + ComError::getComError(hr));
	}

	CComPtr<IWbemServices>& wmi_service::get() {
		if (!is_initialized) {
			CComPtr<IWbemLocator> locator;
			HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator));
			if (FAILED(hr))
				throw wmi_exception(hr, "CoCreateInstance for CLSID_WbemAdministrativeLocator failed!");

			CComBSTR strNamespace(utf8::cvt<std::wstring>(ns).c_str());
			CComBSTR strUser(utf8::cvt<std::wstring>(username).c_str());
			CComBSTR strPassword(utf8::cvt<std::wstring>(password).c_str());

			// @todo: WBEM_FLAG_CONNECT_USE_MAX_WAIT
			hr = locator->ConnectServer(strNamespace, username.empty() ? NULL : strUser, password.empty() ? NULL : strPassword, NULL, NULL, 0, NULL, &service);
			if (FAILED(hr))
				throw wmi_exception(hr, "ConnectServer failed: namespace=" + ns + ", user=" + username);

			set_proxy_blanket(service, username, password);
			is_initialized = true;
		}
		return service;
	}

	std::string row::get_string(const std::string col) {
		CComBSTR bCol(utf8::cvt<std::wstring>(col).c_str());
		CComVariant vValue;
		HRESULT hr = row_obj->Get(bCol, 0, &vValue, 0, 0);
		if (FAILED(hr))
			throw wmi_exception(hr, "Failed to get value for " + col);
		if (vValue.vt == VT_NULL)
			return "<NULL>";

		if (vValue.vt == (VT_ARRAY | VT_BSTR)) {
			long begin, end;
			CComSafeArray<BSTR> arr(vValue.parray);
			begin = arr.GetLowerBound();
			end = arr.GetUpperBound();
			std::stringstream ss;
			ss << "[";
			for (long index = begin; index <= end; index++) {
				CComBSTR bColumn = arr.GetAt(index);
				ss << utf8::cvt<std::string>(bColumn.m_str) << ", ";
			}
			ss << "]";
			return ss.str();
		}
		hr = vValue.ChangeType(VT_BSTR);
		if (FAILED(hr))
			throw wmi_exception(hr, "Failed to convert " + col + " to string");
		std::wstring wstr = vValue.bstrVal;
		return utf8::cvt<std::string>(wstr);
	}

	std::string row::to_string() {
		std::stringstream ss;
		bool first = true;
		BOOST_FOREACH(const std::string c, columns) {
			if (first)
				first = false;
			else
				ss << ", ";
			try {
				ss << get_string(c);
			} catch (...) {
				ss << "???";
			}
		}
		return ss.str();
	}

	long long row::get_int(const std::string col) {
		CComBSTR bCol(utf8::cvt<std::wstring>(col).c_str());
		CComVariant vValue;
		HRESULT hr = row_obj->Get(bCol, 0, &vValue, 0, 0);
		if (FAILED(hr))
			throw wmi_exception(hr, "Failed to get value for " + col);
		hr = vValue.ChangeType(VT_I8);
		if (SUCCEEDED(hr))
			return vValue.llVal;
		hr = vValue.ChangeType(VT_UI8);
		if (SUCCEEDED(hr))
			return vValue.ullVal;
		throw wmi_exception(hr, "Failed to convert " + col + " to number");
	}

	bool row_enumerator::has_next() {
		ULONG retcnt;
		if (row_instance.row_obj)
			row_instance.row_obj.Release();
		HRESULT hr = enumerator_obj->Next(WBEM_INFINITE, 1L, &row_instance.row_obj, &retcnt);
		if (FAILED(hr))
			throw wmi_exception(hr, "Enumeration failed: " + ComError::getComError(hr));
		return hr == WBEM_S_NO_ERROR;
	}

	row& row_enumerator::get_next() {
		return row_instance;
	}

	std::list<std::string> header_enumerator::get() {
		std::list<std::string> ret;

		ULONG retcnt;
		CComPtr<IWbemClassObject> row;
		HRESULT hr = enumerator_obj->Next(WBEM_INFINITE, 1L, &row, &retcnt);
		if (FAILED(hr) || !row)
			throw wmi_exception(hr, "Failed to enumerate columns: " + ComError::getComError(hr));
		if (retcnt == 0)
			throw wmi_exception(ERROR_SUCCESS, "We got no results");

		SAFEARRAY* pstrNames;
		hr = row->GetNames(NULL, WBEM_FLAG_ALWAYS | WBEM_FLAG_NONSYSTEM_ONLY, NULL, &pstrNames);
		if (FAILED(hr))
			throw wmi_exception(hr, "Failed to fetch columns:" + ComError::getComError(hr));

		long begin, end;
		CComSafeArray<BSTR> arr = pstrNames;
		begin = arr.GetLowerBound();
		end = arr.GetUpperBound();
		for (long index = begin; index <= end; index++) {
			CComBSTR bColumn = arr.GetAt(index);
			std::string column = utf8::cvt<std::string>(bColumn.m_str);
			ret.push_back(column);
		}
		return ret;
	}
	row_enumerator query::execute() {
		row_enumerator ret(columns);
		BSTR strQL = _T("WQL");
		CComBSTR strQuery(utf8::cvt<std::wstring>(wql_query).c_str());

		HRESULT hr = instance.get()->ExecQuery(strQL, strQuery, WBEM_FLAG_FORWARD_ONLY, NULL, &ret.enumerator_obj);
		if (FAILED(hr))
			throw wmi_exception(hr, "ExecQuery of '" + wql_query + "' failed: " + utf8::cvt<std::string>(ComError::getWMIError(hr)) + ", " + ComError::getComError(hr));
		return ret;
	}

	row_enumerator classes::get() {
		row_enumerator ret(columns);
		CComBSTR strSuperClass = utf8::cvt<std::wstring>(super_class).c_str();
		CComPtr<IEnumWbemClassObject> enumerator;
		HRESULT hr = instance.get()->CreateClassEnum(strSuperClass, WBEM_FLAG_DEEP | WBEM_FLAG_USE_AMENDED_QUALIFIERS | WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, NULL, &ret.enumerator_obj);
		if (FAILED(hr))
			throw wmi_exception(hr, "CreateInstanceEnum failed: " + ComError::getComError(hr) + ")");
		return ret;
	}

	row_enumerator instances::get() {
		row_enumerator ret(columns);
		CComBSTR strSuperClass = utf8::cvt<std::wstring>(super_class).c_str();
		CComPtr<IEnumWbemClassObject> enumerator;
		HRESULT hr = instance.get()->CreateInstanceEnum(strSuperClass, WBEM_FLAG_SHALLOW | WBEM_FLAG_USE_AMENDED_QUALIFIERS | WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, NULL, &ret.enumerator_obj);
		if (FAILED(hr))
			throw wmi_exception(hr, "CreateInstanceEnum failed: " + ComError::getComError(hr));
		return ret;
	}

	std::list<std::string> query::get_columns() {
		if (!columns.empty())
			return columns;
		BSTR strQL = _T("WQL");
		CComBSTR strQuery(utf8::cvt<std::wstring>(wql_query).c_str());

		header_enumerator enumerator;
		HRESULT hr = instance.get()->ExecQuery(strQL, strQuery, WBEM_FLAG_PROTOTYPE, NULL, &enumerator.enumerator_obj);
		if (FAILED(hr)) {
			std::string prefix = "Failed to execute query: ";
			if (hr == WBEM_E_INVALID_QUERY) {
				prefix = "Invalid query " + wql_query;
			}
			throw wmi_exception(hr, prefix + ComError::getComError(hr));
		}
		columns = enumerator.get();
		return columns;
	}

	std::string ComError::getComError(HRESULT hr) {
		return error::com::get(hr);
	}
}