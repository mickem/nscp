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

#include <strEx.h>
#include <checkHelpers.hpp>
#ifdef USE_BOOST
#include <boost/regex.hpp>
#else
#pragma message("Compiling a module without regular expression support")
#endif

namespace filters {

	class filter_exception {
		std::wstring error_;
	public:
		filter_exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};
	struct parse_exception : public filter_exception {
		parse_exception(std::wstring error) : filter_exception(error) {}
	};
	namespace filter {
		struct sub_string_filter {
			static bool filter(std::wstring filter, std::wstring str) {
				return str.find(filter) != std::wstring::npos;
			}
		};
		struct exact_string_filter {
			static bool filter(std::wstring filter, std::wstring str) {
				return str == filter;
			}
		};
		struct not_string_filter {
			static bool filter(std::wstring filter, std::wstring str) {
				return !(str == filter);
			}
		};
#ifdef USE_BOOST
		struct regexp_string_filter {
			static bool filter(boost::wregex filter, std::wstring str) {
				return  boost::regex_match(str, filter);
			}
		};
#endif
		template <typename TType>
		struct numeric_max_filter {
			static bool filter(TType filter, TType value) {
				//std::wcout << filter << _T(" >= ") << value << std::endl;
				return value >= filter;
			}
		};
		template <typename TType>
		struct numeric_min_filter {
			static bool filter(TType filter, TType value) {
				return value <= filter;
			}
		};
		template <typename TType>
		struct numeric_equals_filter {
			static bool filter(TType filter, TType value) {
				return value == filter;
			}
		};
		template <typename TType>
		struct numeric_nequals_filter {
			static bool filter(TType filter, TType value) {
				return value != filter;
			}
		};
		template <typename TType>
		struct always_true_filter {
			static bool filter(TType filter, TType value) {
				return true;
			}
		};
		template <typename TListType, typename TType>
		struct numeric_inlist_filter {
			static bool filter(const TListType &filter, const TType value) {
				for (TListType::const_iterator it = filter.begin(); it != filter.end(); ++it) {
					if ((*it) == value)
						return true;
				}
				return false;
			}
		};
	}
	namespace handlers {
		struct handler_exception : public filter_exception {
			handler_exception(std::wstring error) : filter_exception(error) {}
		};
		struct string_handler {
			static std::wstring parse(std::wstring str) {
				return str;
			}
			static std::wstring print(std::wstring value) {
				return value;
			}
		};
		template<class TType, class TSubHandler>
		struct numeric_list_handler {
			static std::list<TType> parse(std::wstring str) {
				std::list<TType> ret;
				std::list<std::wstring> tmp = strEx::splitEx(str, _T(","));
				for (std::list<std::wstring>::const_iterator it = tmp.begin(); it != tmp.end(); ++it) {
					ret.push_back(TSubHandler::parse(*it));
				}
				return ret;
			}
		};
#ifdef USE_BOOST
		struct regexp_handler {
			static boost::wregex parse(std::wstring str) {
				try {
					return boost::wregex(str);
				} catch (const boost::bad_expression e) {
					throw handler_exception(_T("Invalid syntax in regular expression:") + str);
				} catch (...) {
					throw handler_exception(_T("Invalid syntax in regular expression:") + str);
				}
			}
		};
#endif
		struct eventtype_handler {
			static unsigned int parse(std::wstring str) {
				if (str == _T("error"))
					return EVENTLOG_ERROR_TYPE;
				if (str == _T("warning"))
					return EVENTLOG_WARNING_TYPE;
				if (str == _T("info"))
					return EVENTLOG_INFORMATION_TYPE;
				if (str == _T("auditSuccess"))
					return EVENTLOG_AUDIT_SUCCESS;
				if (str == _T("auditFailure"))
					return EVENTLOG_AUDIT_FAILURE;
				return strEx::stoi(str);
			}
			static std::wstring toString(unsigned int dwType) {
				if (dwType == EVENTLOG_ERROR_TYPE)
					return _T("error");
				if (dwType == EVENTLOG_WARNING_TYPE)
					return _T("warning");
				if (dwType == EVENTLOG_INFORMATION_TYPE)
					return _T("info");
				if (dwType == EVENTLOG_AUDIT_SUCCESS)
					return _T("auditSuccess");
				if (dwType == EVENTLOG_AUDIT_FAILURE)
					return _T("auditFailure");
				return strEx::itos(dwType);
			}		
		};
		struct eventseverity_handler {
			static unsigned int parse(std::wstring str) {
				if (str == _T("success"))
					return 0;
				if (str == _T("informational"))
					return 1;
				if (str == _T("warning"))
					return 2;
				if (str == _T("error"))
					return 3;
				return strEx::stoi(str);
			}
			static std::wstring toString(unsigned int dwType) {
				if (dwType == 0)
					return _T("success");
				if (dwType == 1)
					return _T("informational");
				if (dwType == 2)
					return _T("warning");
				if (dwType == 3)
					return _T("error");
				return strEx::itos(dwType);
			}		
		};	}

	template <typename TFilterType, typename TValueType, class THandler, class TFilter>
	struct filter_one {
		TFilterType filter_;
		bool hasFilter_;
		std::wstring value_;
		filter_one() : hasFilter_(false) {}
		filter_one(const filter_one &other) : hasFilter_(other.hasFilter_), filter_(other.filter_), value_(other.value_) {
		}

		inline bool hasFilter() const {
			return hasFilter_;
		}
		bool matchFilter(const TValueType value) const {
			return TFilter::filter(filter_, value);
		}
		const filter_one & operator=(std::wstring value) {
			value_ = value;
			hasFilter_ = false;
			try {
				filter_ = THandler::parse(value);
				hasFilter_ = true;
			} catch (handlers::handler_exception e) {
				throw parse_exception(e.getMessage() + _T(": ") + value);
			} catch (...) {
				throw parse_exception(_T("Unknown parse exception: ") + value);
			}
			return *this;
		}
		std::wstring getValue() const {
			return value_;
		}
	};

	typedef filter_one<std::wstring, std::wstring, handlers::string_handler, filter::sub_string_filter> sub_string_filter;
#ifdef USE_BOOST
	typedef filter_one<boost::wregex, std::wstring, handlers::regexp_handler, filter::regexp_string_filter> regexp_string_filter;
#endif
	typedef filter_one<std::wstring, std::wstring, handlers::string_handler, filter::exact_string_filter> exact_string_filter;
	typedef filter_one<std::wstring, std::wstring, handlers::string_handler, filter::not_string_filter> not_string_filter;

	struct filter_all_strings {
		sub_string_filter sub;
		exact_string_filter exact;
		not_string_filter not;
		std::wstring value_;
		typedef std::wstring TValueType;
#ifdef USE_BOOST
		regexp_string_filter regexp;
#endif
		filter_all_strings() {}

		inline bool hasFilter() const {
			return sub.hasFilter() 
#ifdef USE_BOOST
				|| regexp.hasFilter() 
#endif
				|| exact.hasFilter()
				|| not.hasFilter()
				;
		}
		bool matchFilter(const std::wstring str) const {
			if ((sub.hasFilter())&&(sub.matchFilter(str)))
				return true;
#ifdef USE_BOOST
			else if ((regexp.hasFilter())&&(regexp.matchFilter(str)))
				return true;
#endif
			else if ((exact.hasFilter())&&(exact.matchFilter(str)))
				return true;
			else if ((not.hasFilter())&&(not.matchFilter(str)))
				return true;
			return false;
		}
		std::wstring getValue() const {
			return value_;
		}
		const filter_all_strings & operator=(std::wstring value) {
			value_ = value;
			strEx::token t = strEx::getToken(value, ':', false);
			if (t.first == _T("substr")) {
				sub = t.second;
#ifdef USE_BOOST
			} else if (t.first == _T("regexp")) {
				regexp = t.second;
#else
			} else if (t.first == _T("regexp")) {
				throw parse_exception(_T("Regular expression support not enabled!") + value);
#endif
			} else if (t.first.length() > 1 && t.first[0] == L'=') {
				exact = t.first.substr(1);
			} else if (t.first.length() > 2 && t.first[0] == L'!' && t.first[1] == L'=') {
				not = t.first.substr(2);
			} else if (t.first.length() > 1 && t.first[0] == L'!') {
				not = t.first.substr(1);
			} else {
				exact = t.first;
			}
			return *this;
		}
		std::wstring to_string() const {
			if (sub.hasFilter())
				return _T("substring: '") + sub.getValue() + _T("'");
#ifdef USE_BOOST
			if (regexp.hasFilter())
				return _T("regexp: '") + regexp.getValue() + _T("'");
#endif
			if (exact.hasFilter())
				return _T("exact: '") + exact.getValue() + _T("'");
			return _T("MISSING VALUE");
		}
	};

	template <typename TType, class THandler>
	struct filter_all_numeric {

		filter_one<TType, TType, THandler, filter::numeric_max_filter<TType> > max;
		filter_one<TType, TType, THandler, filter::numeric_min_filter<TType> > min;
		filter_one<TType, TType, THandler, filter::numeric_equals_filter<TType> > eq;
		filter_one<TType, TType, THandler, filter::numeric_nequals_filter<TType> > neq;
		filter_one<std::list<TType>, TType, handlers::numeric_list_handler<TType, THandler>, filter::numeric_inlist_filter<std::list<TType>, TType> > inList;
		std::wstring value_;

		filter_all_numeric() {}
		filter_all_numeric(const filter_all_numeric &other) {
			max = other.max;
			min = other.min;
			eq = other.eq;
			neq = other.neq;
			inList = other.inList;
		}
		inline bool hasFilter() const {
			return max.hasFilter() || min.hasFilter() || eq.hasFilter() || neq.hasFilter() || inList.hasFilter();
		}
		bool matchFilter(const TType value) const {
			if ((max.hasFilter())&&(max.matchFilter(value)))
				return true;
			else if ((min.hasFilter())&&(min.matchFilter(value)))
				return true;
			else if ((eq.hasFilter())&&(eq.matchFilter(value)))
				return true;
			else if ((neq.hasFilter())&&(neq.matchFilter(value)))
				return true;
			else if ((inList.hasFilter())&&(inList.matchFilter(value)))
				return true;
			return false;
		}
		const filter_all_numeric& operator=(std::wstring value) {
			value_ = value;
			if (value.substr(0,1) == _T(">")) {
				max = value.substr(1);
			} else if (value.substr(0,2) == _T("<>")) {
				neq = value.substr(2);
			} else if (value.substr(0,1) == _T("<")) {
				min = value.substr(1);
			} else if (value.substr(0,1) == _T("=")) {
				eq = value.substr(1);
			} else if (value.substr(0,2) == _T("!=")) {
				neq = value.substr(2);
			} else if (value.substr(0,1) == _T("!")) {
				neq = value.substr(1);
			} else if (value.substr(0,3) == _T("in:")) {
				inList = value.substr(3);
			} else if (value.substr(0,3) == _T("gt:")) {
				max = value.substr(3);
			} else if (value.substr(0,3) == _T("lt:")) {
				min = value.substr(3);
			} else if (value.substr(0,3) == _T("ne:")) {
				neq = value.substr(3);
			} else if (value.substr(0,3) == _T("eq:")) {
				eq = value.substr(3);
			} else {
				throw parse_exception(_T("Unknown filter key: ") + value + _T(" (numeric filters have to have an operator as well ie. foo=>5 or bar==5)"));
			}
			return *this;
		}
#define NSCP_FF_DEBUG_NUM(key) if (key.hasFilter()) strEx::append_list(str, std::wstring(_T( # key )) + _T(" ") + key.getValue(), _T(","));
		std::wstring to_string() const {
			std::wstring str;
			NSCP_FF_DEBUG_NUM(max);
			NSCP_FF_DEBUG_NUM(min);
			NSCP_FF_DEBUG_NUM(eq);
			NSCP_FF_DEBUG_NUM(neq);
			NSCP_FF_DEBUG_NUM(inList);
			return str;
		}
		std::wstring getValue() const {
			return value_;
		}
	};
	typedef filter_all_numeric<__int64, checkHolders::time_handler<__int64> > filter_all_times;
	typedef filter_all_numeric<unsigned long, checkHolders::int_handler > filter_all_num_ul;
	typedef filter_all_numeric<long long, checkHolders::int_handler > filter_all_num_ll;

	template <typename TFilterType, typename TValueType>
	struct chained_filter {
		enum filter_mode {
			plus = 1,
			minus = 2,
			normal = 3,
		};
		typedef std::pair<filter_mode,TFilterType> filteritem_type;
		typedef std::list<filteritem_type> filterlist_type;

		filterlist_type chain;
		bool filterAll;
		
		chained_filter() : filterAll(false) {}

		void push_filter(std::wstring key, TFilterType filter) {
			filter_mode mode = normal;
			if (key.substr(0,1) == _T("+"))
				mode = plus;
			if (key.substr(0,1) == _T("-"))
				mode = minus;
			chain.push_back(filteritem_type(mode, filter));
		}

		bool empty() {
			return chain.empty();
		}

		bool hasFilter() const {
			return !chain.empty();
		}
		bool get_inital_state() const {
			return filterAll;
		}

		bool match(bool state, const TValueType item) {
			bool matched = state;
			for (filterlist_type::const_iterator cit = chain.begin(); cit != chain.end(); ++cit ) {
				int mode = (*cit).first;
				bool bTmpMatched = (*cit).second.matchFilter(item);
				if ((mode == minus)&&(bTmpMatched)) {
					// a -<filter> hit so thrash result!
					matched = false;
					break;
				} else if ((mode == plus)&&(!bTmpMatched)) {
					// a +<filter> missed hit so thrash result!
					matched = false;
					break;
				} else if (bTmpMatched) {
					matched = true;
				}
			}
			return matched;
		}
		std::wstring mode_2_string(filter_mode mode) const {
			if (mode == plus)
				return _T("+");
			if (mode == minus)
				return _T("-");
			if (mode == normal)
				return _T(".");
			return _T("?");
		}
		std::wstring debug() const {
			std::wstringstream ss;
			ss << _T("Initial state: ") << get_inital_state() << std::endl;
			ss << _T("filters: ") << std::endl;
			for (filterlist_type::const_iterator cit = chain.begin(); cit != chain.end(); ++cit )
				ss << _T("  ") << mode_2_string((*cit).first) << _T(": ") << (*cit).second.to_string() << std::endl;
			return ss.str();
		}

	};


}
