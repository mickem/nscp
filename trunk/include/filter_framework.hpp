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
#ifndef NO_BOOST_DEP
#include <boost/regex.hpp>
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
#ifndef NO_BOOST_DEP
		struct regexp_string_filter {
			static bool filter(boost::wregex filter, std::wstring str) {
				return  boost::regex_match(str, filter);
			}
		};
#endif
		template <typename TType>
		struct numeric_max_filter {
			static bool filter(TType filter, TType value) {
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
#ifndef NO_BOOST_DEP
		struct regexp_handler {
			static boost::wregex parse(std::wstring str) {
				try {
					return boost::wregex(str);
				} catch (const boost::bad_expression e) {
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
		TFilterType filter;
		bool hasFilter_;
		std::wstring value_;
		filter_one() : hasFilter_(false) {}
		filter_one(const filter_one &other) : hasFilter_(other.hasFilter_), filter(other.filter) {
		}

		inline bool hasFilter() const {
			return hasFilter_;
		}
		bool matchFilter(const TValueType value) const {
			return TFilter::filter(filter, value);
		}
		const filter_one & operator=(std::wstring value) {
			value_ = value;
			hasFilter_ = false;
			try {
				filter = THandler::parse(value);
				hasFilter_ = true;
			} catch (handlers::handler_exception e) {
				throw parse_exception(e.getMessage());
			}
			return *this;
		}
		std::wstring getValue() const {
			return value_;
		}
	};

	typedef filter_one<std::wstring, std::wstring, handlers::string_handler, filter::sub_string_filter> sub_string_filter;
#ifndef NO_BOOST_DEP
	typedef filter_one<boost::wregex, std::wstring, handlers::regexp_handler, filter::regexp_string_filter> regexp_string_filter;
#endif
	typedef filter_one<std::wstring, std::wstring, handlers::string_handler, filter::exact_string_filter> exact_string_filter;

	struct filter_all_strings {
		sub_string_filter sub;
		exact_string_filter exact;
		std::wstring value_;
#ifndef NO_BOOST_DEP
		regexp_string_filter regexp;
#endif
		filter_all_strings() {}

		inline bool hasFilter() const {
			return sub.hasFilter() 
#ifndef NO_BOOST_DEP
				|| regexp.hasFilter() 
#endif
				|| exact.hasFilter();
		}
		bool matchFilter(const std::wstring str) const {
			if ((sub.hasFilter())&&(sub.matchFilter(str)))
				return true;
#ifndef NO_BOOST_DEP
			else if ((regexp.hasFilter())&&(regexp.matchFilter(str)))
				return true;
#endif
			else if ((exact.hasFilter())&&(exact.matchFilter(str)))
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
#ifndef NO_BOOST_DEP
			} else if (t.first == _T("regexp")) {
				regexp = t.second;
#endif
			} else {
				exact = t.first;
			}
			return *this;
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
			} else if (value.substr(0,1) == _T("<")) {
				min = value.substr(1);
			} else if (value.substr(0,1) == _T("=")) {
				eq = value.substr(1);
			} else if (value.substr(0,2) == _T("!=")) {
				neq = value.substr(2);
			} else if (value.substr(0,2) == _T("<>")) {
				neq = value.substr(2);
			} else if (value.substr(0,1) == _T("!")) {
				neq = value.substr(1);
			} else if (value.substr(0,3) == _T("in:")) {
				inList = value.substr(3);
			} else {
				throw parse_exception(_T("Unknown filter key: ") + value + _T(" (numeric filters have to have an operator as well ie. foo=>5 or bar==5)"));
			}
			return *this;
		}
		std::wstring getValue() const {
			return value_;
		}
	};
	typedef filter_all_numeric<unsigned long long, checkHolders::time_handler<unsigned long long> > filter_all_times;

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

		bool hasFilter() {
			return !chain.empty();
		}
		bool get_inital_state() {
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

	};


}