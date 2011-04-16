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
#pragma once

#include <string>
#include <map>

#include <boost/lexical_cast.hpp>

#include <strEx.h>
#include <error.hpp>
#include <filter_framework.hpp>
#include <WbemCli.h>

class ComError {
public:
	static std::wstring getWMIError(HRESULT hres) {
		switch (hres) {
			case WBEM_E_ACCESS_DENIED:
				return _T("The current user does not have permission to view the result set.");
			case WBEM_E_FAILED:
				return _T("This indicates other unspecified errors.");
			case WBEM_E_INVALID_PARAMETER:
				return _T("An invalid parameter was specified.");
			case WBEM_E_INVALID_QUERY:
				return _T("The query was not syntactically valid.");
			case WBEM_E_INVALID_QUERY_TYPE:
				return _T("The requested query language is not supported.");
			case WBEM_E_OUT_OF_MEMORY:
				return _T("There was not enough memory to complete the operation.");
			case WBEM_E_SHUTTING_DOWN:
				return _T("Windows Management service was stopped and restarted. A new call to ConnectServer is required.");
			case WBEM_E_TRANSPORT_FAILURE:
				return _T("This indicates the failure of the remote procedure call (RPC) link between the current process and Windows Management.");
			case WBEM_E_NOT_FOUND:
				return _T("The query specifies a class that does not exist.");
			default:
				return _T("");
		}
	}
	static std::wstring getComError(std::wstring inDesc = _T("")) {
		USES_CONVERSION;
		CComPtr<IErrorInfo> errorInfo;
		HRESULT hr = GetErrorInfo(NULL, &errorInfo);
		if (FAILED(hr) || hr == S_FALSE)
			return _T("unknown error: ") + error::format::from_system(hr);
		CComBSTR bDesc, bSource;
		std::wstring src = _T("unknown");
		hr = errorInfo->GetSource(&bSource);
		if (SUCCEEDED(hr))
			src = OLE2T(bSource);
		std::wstring desc;
		hr = errorInfo->GetDescription(&bDesc);
		if (SUCCEEDED(hr))
			desc = OLE2T(bDesc);
		if (desc.empty() && !inDesc.empty())
			desc = inDesc;
		else if (desc.empty())
			desc = _T("unknown error: ") + error::format::from_system(hr);
		return src + _T(" - ") + desc;
	}
};

class WMIException {
	std::wstring message_;
public:
	WMIException(std::wstring str, HRESULT code) {
		message_ = str + _T(":") + error::format::from_system(code);
	}
	WMIException(std::wstring str) {
		message_ = str;
	}
	std::wstring getMessage() {
		return message_;
	}
};
class WMIQuery
{
public:
	struct WMIResult {
		std::wstring alias;
		std::wstring string;
		long long numeric;
		bool isNumeric;
		WMIResult() : isNumeric(false), numeric(0) {}
		void setString(std::wstring a, std::wstring s) {
			string = s;
			numeric = boost::lexical_cast<long long>(s);
			alias = a;
		}
		void setNumeric(std::wstring a, long long n) {
			numeric = n;
			string = boost::lexical_cast<std::wstring>(n);
			alias = a;
		}
		void setBoth(std::wstring a, long long n, std::wstring s) {
			numeric = n;
			string = s;
			alias = a;
		}
	};
	struct wmi_row {
		typedef std::map<std::wstring,WMIResult> list_type;
		list_type results;
		boolean hasAlias(std::wstring alias) const {
			if (alias.empty())
				return true;
			return results.find(alias) != results.end();
		}
		const WMIResult get(std::wstring alias) const {
			WMIResult ret;
			list_type::const_iterator cit = results.find(alias);
			if (cit != results.end())
				ret = (*cit).second;
			return ret;
		}
		void addValue(std::wstring column, WMIResult value) {
			results[column] = value;
		}

		std::wstring render(std::wstring syntax = _T(""), std::wstring sep = _T(", ")) {
			std::wstring ret;
			for (list_type::const_iterator it = results.begin(); it != results.end(); ++it) {
				if (syntax.empty()) {
					if (!ret.empty())	ret += sep;
					ret += (*it).first + _T("=") + (*it).second.string;
				} else {
					std::wstring sub = syntax;
					strEx::replace(sub, _T("%column%"), (*it).first);
					strEx::replace(sub, _T("%value%"), (*it).second.string);
					strEx::replace(sub, _T("%") + (*it).first + _T("%"), (*it).second.string);
					if (sub == syntax)
						continue;
					strEx::append_list(ret, sub, sep);
				}
			}
			return ret;
		}

	};
	typedef std::list<wmi_row> result_type;
	struct wmi_filter {
		std::wstring alias;
		filters::filter_all_strings string;
		filters::filter_all_numeric<unsigned long long, checkHolders::int64_handler >  numeric;

		inline bool hasFilter() {
			return string.hasFilter() || numeric.hasFilter();
		}
		bool matchFilter(const wmi_row &value) const {
			if (!value.hasAlias(alias)) {
				NSC_DEBUG_MSG_STD(_T("We don't have any column matching: ") + alias);
				return false;
			}
			if (alias.empty()) {
				for (wmi_row::list_type::const_iterator cit = value.results.begin(); cit != value.results.end(); ++cit) {
					if ((string.hasFilter())&&(string.matchFilter((*cit).second.string)))
						return true;
					else if ((numeric.hasFilter())&&(numeric.matchFilter((*cit).second.numeric)))
						return true;
				}
			} else {
				if ((string.hasFilter())&&(string.matchFilter(value.get(alias).string)))
					return true;
				else if ((numeric.hasFilter())&&(numeric.matchFilter(value.get(alias).numeric)))
					return true;
			}
			NSC_DEBUG_MSG_STD(_T("Value did not match a filter: ") + alias);
			return false;
		}
	};
	WMIQuery(void) {};
	~WMIQuery(void) {};

	result_type  execute(std::wstring ns, std::wstring query);
	std::wstring sanitize_string(LPTSTR in);
};
