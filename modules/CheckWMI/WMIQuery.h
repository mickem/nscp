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
#include <WbemCli.h>

#include <atlbase.h>
//#include <atlcom.h>
//#include <atlstr.h>
#include <atlsafe.h>

#include <objidl.h>

namespace wmi_impl {
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
		static std::string getComError(std::wstring inDesc = _T(""));
	};

	class wmi_exception : public std::exception {
		std::string message_;
	public:
		wmi_exception(std::string str, HRESULT code) {
			message_ = str + ":" + error::format::from_system(code);
		}
		wmi_exception(std::string str) {
			message_ = str;
		}
		~wmi_exception() throw() {}
		const char* what() const throw() {
			return reason().c_str();
		}

		std::string reason() const throw() {
			return message_;
		}
	};

	struct row {
		CComPtr<IWbemClassObject> row_obj;

		std::string get_string(const std::string col);
		long long get_int(const std::string col);
	};

	struct row_enumerator {
		row row_instance;
		CComPtr<IEnumWbemClassObject> enumerator_obj;
		bool has_next();
		row& get_next();

	};

	struct query {

		CComPtr<IWbemServices> service;

		std::string wql_query;
		std::string ns;
		std::string username;
		std::string password;
		query(std::string wql_query, std::string ns, std::string username, std::string password) : wql_query(wql_query), ns(ns), username(username), password(password) {}

		void init_query();
		std::list<std::string> get_columns();
		row_enumerator execute();



	};

	class WMIQuery
	{
	public:
		struct WMIResult {
		public:
			std::wstring string;
			long long numeric;
			bool isNumeric;
			WMIResult() : isNumeric(false), numeric(0) {}
			void set_raw_str(std::wstring str) {
				if (!str.empty()) {
					std::string::size_type pos = str.find_last_not_of(_T(" "));
					if (pos != std::string::npos)
						str.erase(pos+1);
					else
						str.erase(str.begin(), str.end());
				}
				string = str;
			}
			void setString(std::wstring s) {
				set_raw_str(s);
				try {
					numeric = boost::lexical_cast<long long>(s);
				} catch (...) {
					numeric = 0;
				}
			}
			void setNumeric(long long n) {
				numeric = n;
				set_raw_str(boost::lexical_cast<std::wstring>(n));
			}
			void setBoth(long long n, std::wstring s) {
				numeric = n;
				set_raw_str(s);
			}
			std::string get_string() const {
				return utf8::cvt<std::string>(string);

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

			std::string render(std::string syntax = "", std::string sep = ", ") {
				std::string ret;
				BOOST_FOREACH(const list_type::value_type &v, results) {
					if (syntax.empty()) {
						if (!ret.empty())	ret += sep;
						ret += utf8::cvt<std::string>(v.first) + "=" + utf8::cvt<std::string>(v.second.string);
					} else {
						std::string sub = syntax;
						strEx::replace(sub, "%column%", utf8::cvt<std::string>(v.first));
						strEx::replace(sub, "%value%", utf8::cvt<std::string>(v.second.string));
						strEx::replace(sub, "%" + utf8::cvt<std::string>(v.first) + "%", utf8::cvt<std::string>(v.second.string));
						if (sub == syntax)
							continue;
						strEx::append_list(ret, sub, sep);
					}
				}
				return ret;
			}

		};

		WMIQuery(void) {};
		~WMIQuery(void) {};

		typedef std::list<wmi_row> result_type;
		std::wstring sanitize_string(LPTSTR in);
		WMIQuery::result_type WMIQuery::get_classes(std::wstring ns, std::wstring superClass, std::wstring user, std::wstring password);
		WMIQuery::result_type WMIQuery::get_instances(std::wstring ns, std::wstring superClass, std::wstring user, std::wstring password);
	};
}