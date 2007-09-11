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

namespace filters {

	class filter_exception {
		std::string error_;
	public:
		filter_exception(std::string error) : error_(error) {}
		std::string getMessage() {
			return error_;
		}
	};
	struct parse_exception : public filter_exception {
		parse_exception(std::string error) : filter_exception(error) {}
	};
	namespace filter {
		struct sub_string_filter {
			static bool filter(std::string filter, std::string str) {
				return str.find(filter) != std::string::npos;
			}
		};
		struct exact_string_filter {
			static bool filter(std::string filter, std::string str) {
				return str == filter;
			}
		};
#ifndef NO_BOOST_DEP
		struct regexp_string_filter {
			static bool filter(boost::regex filter, std::string str) {
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
			handler_exception(std::string error) : filter_exception(error) {}
		};
		struct string_handler {
			static std::string parse(std::string str) {
				return str;
			}
		};
		template<class TType, class TSubHandler>
		struct numeric_list_handler {
			static std::list<TType> parse(std::string str) {
				std::list<TType> ret;
				std::list<std::string> tmp = strEx::splitEx(str, ",");
				for (std::list<std::string>::const_iterator it = tmp.begin(); it != tmp.end(); ++it) {
					ret.push_back(TSubHandler::parse(*it));
				}
				return ret;
			}
		};
#ifndef NO_BOOST_DEP
		struct regexp_handler {
			static boost::regex parse(std::string str) {
				try {
					return boost::regex(str);
				} catch (const boost::bad_expression e) {
					throw handler_exception("Invalid syntax in regular expression:" + str);
				}
			}
		};
#endif
		struct eventtype_handler {
			static unsigned int parse(std::string str) {
				if (str == "error")
					return EVENTLOG_ERROR_TYPE;
				if (str == "warning")
					return EVENTLOG_WARNING_TYPE;
				if (str == "info")
					return EVENTLOG_INFORMATION_TYPE;
				if (str == "auditSuccess")
					return EVENTLOG_AUDIT_SUCCESS;
				if (str == "auditFailure")
					return EVENTLOG_AUDIT_FAILURE;
				return strEx::stoi(str);
			}
			static std::string toString(unsigned int dwType) {
				if (dwType == EVENTLOG_ERROR_TYPE)
					return "error";
				if (dwType == EVENTLOG_WARNING_TYPE)
					return "warning";
				if (dwType == EVENTLOG_INFORMATION_TYPE)
					return "info";
				if (dwType == EVENTLOG_AUDIT_SUCCESS)
					return "auditSuccess";
				if (dwType == EVENTLOG_AUDIT_FAILURE)
					return "auditFailure";
				return strEx::itos(dwType);
			}		
		};
		struct eventseverity_handler {
			static unsigned int parse(std::string str) {
				if (str == "success")
					return 0;
				if (str == "informational")
					return 1;
				if (str == "warning")
					return 2;
				if (str == "error")
					return 3;
				return strEx::stoi(str);
			}
			static std::string toString(unsigned int dwType) {
				if (dwType == 0)
					return "success";
				if (dwType == 1)
					return "informational";
				if (dwType == 2)
					return "warning";
				if (dwType == 3)
					return "error";
				return strEx::itos(dwType);
			}		
		};	}

	template <typename TFilterType, typename TValueType, class THandler, class TFilter>
	struct filter_one {
		TFilterType filter;
		bool hasFilter_;
		filter_one() : hasFilter_(false) {}
		filter_one(const filter_one &other) : hasFilter_(other.hasFilter_), filter(other.filter) {
		}

		inline bool hasFilter() const {
			return hasFilter_;
		}
		bool matchFilter(const TValueType value) const {
			return TFilter::filter(filter, value);
		}
		const filter_one & operator=(std::string value) {
			hasFilter_ = false;
			try {
				filter = THandler::parse(value);
				hasFilter_ = true;
			} catch (handlers::handler_exception e) {
				throw parse_exception(e.getMessage());
			}
			return *this;
		}
	};

	typedef filter_one<std::string, std::string, handlers::string_handler, filter::sub_string_filter> sub_string_filter;
#ifndef NO_BOOST_DEP
	typedef filter_one<boost::regex, std::string, handlers::regexp_handler, filter::regexp_string_filter> regexp_string_filter;
#endif
	typedef filter_one<std::string, std::string, handlers::string_handler, filter::exact_string_filter> exact_string_filter;

	struct filter_all_strings {
		sub_string_filter sub;
		exact_string_filter exact;
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
		bool matchFilter(const std::string str) const {
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
		const filter_all_strings & operator=(std::string value) {
			strEx::token t = strEx::getToken(value, ':', false);
			if (t.first == "substr") {
				sub = t.second;
#ifndef NO_BOOST_DEP
			} else if (t.first == "regexp") {
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
		const filter_all_numeric& operator=(std::string value) {
			if (value.substr(0,1) == ">") {
				max = value.substr(1);
			} else if (value.substr(0,1) == "<") {
				min = value.substr(1);
			} else if (value.substr(0,1) == "=") {
				eq = value.substr(1);
			} else if (value.substr(0,2) == "!=") {
				neq = value.substr(2);
			} else if (value.substr(0,3) == "in:") {
				inList = value.substr(3);
			} else {
				throw parse_exception("Unknown filter key: " + value);
			}
			return *this;
		}
	};
	typedef filter_all_numeric<unsigned long long, checkHolders::time_handler<unsigned long long> > filter_all_times;
}