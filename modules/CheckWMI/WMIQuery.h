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
#include <strEx.h>
#include <error.hpp>
#include <filter_framework.hpp>

class ComError {
public:
	static std::wstring getComError() {
		USES_CONVERSION;
		CComPtr<IErrorInfo> errorInfo;
		HRESULT hr = GetErrorInfo(NULL, &errorInfo);
		if (FAILED(hr) || hr == S_FALSE)
			return _T("unknown error: ") + error::format::from_system(hr);
		CComBSTR bDesc, bSource;
		hr = errorInfo->GetSource(&bSource);
		if (FAILED(hr))
			return _T("unknown error: ") + error::format::from_system(hr);
		hr = errorInfo->GetDescription(&bDesc);
		if (FAILED(hr))
			return _T("unknown error: ") + error::format::from_system(hr);
		std::wstring ret = OLE2T(bSource);
		ret += _T(" - ");
		ret += OLE2T(bDesc);
		return ret;
	}
};

class WMIException {
	std::wstring message_;
public:
	WMIException(std::wstring str, HRESULT code) {
		message_ = str + _T(":") + error::format::from_system(code);
	}
	std::wstring getMessage() {
		return message_;
	}
};
class WMIQuery
{
private:
	bool bInitialized;

public:
	struct WMIResult {
		std::wstring alias;
		std::wstring string;
		long long numeric;
		bool isNumeric;
		WMIResult() : isNumeric(false), numeric(0) {}
		void setString(std::wstring a, std::wstring s) {
			string = s;
			numeric = 0;
			alias = a;
		}
		void setNumeric(std::wstring a, long long n) {
			numeric = n;
			string = strEx::itos(n);
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

		std::wstring render() {
			std::wstring ret;
			for (list_type::const_iterator it = results.begin(); it != results.end(); ++it) {
				if (!ret.empty())	ret += _T(", ");
				ret += (*it).first + _T("=") + (*it).second.string;
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
	WMIQuery(void);
	~WMIQuery(void);

	result_type  execute(std::wstring query);
	std::wstring sanitize_string(LPTSTR in);

	bool initialize();
	void unInitialize();
};
