#pragma once

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
		struct regexp_string_filter {
			static bool filter(boost::regex filter, std::string str) {
				return  boost::regex_match(str, filter);
			}
		};
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
				return value = filter;
			}
		};
		template <typename TType>
		struct numeric_nequals_filter {
			static bool filter(TType filter, TType value) {
				return value != filter;
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
		struct regexp_handler {
			static boost::regex parse(std::string str) {
				try {
					return boost::regex(str);
				} catch (const boost::bad_expression e) {
					throw handler_exception("Invalid syntax in regular expression:" + str);
				}
			}
		};
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
	}

	template <typename TFilterType, typename TValueType, class THandler, class TFilter>
	struct filter_one {
		TFilterType filter;
		bool hasFilter_;
		filter_one() : hasFilter_(false) {}

		inline bool hasFilter() const {
			return hasFilter_;
		}
		bool matchFilter(TValueType str) const {
			return TFilter::filter(filter, str);
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
	typedef filter_one<boost::regex, std::string, handlers::regexp_handler, filter::regexp_string_filter> regexp_string_filter;

	struct filter_all_strings {
		sub_string_filter sub;
		regexp_string_filter regexp;
		filter_all_strings() {}

		inline bool hasFilter() const {
			return sub.hasFilter() || regexp.hasFilter();
		}
		bool matchFilter(std::string str) const {
			if ((regexp.hasFilter())&&(regexp.matchFilter(str)))
				return true;
			else if ((sub.hasFilter())&&(sub.matchFilter(str)))
				return true;
			return false;
		}
		const filter_all_strings & operator=(std::string value) {
			strEx::token t = strEx::getToken(value, ':', false);
			if (t.first == "regexp") {
				regexp = t.second;
			} else if (t.first == "substr") {
				sub = t.second;
			} else {
				sub = t.second;
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

		inline bool hasFilter() const {
			return max.hasFilter() || min.hasFilter() || eq.hasFilter() || neq.hasFilter();
		}
		bool matchFilter(TType value) const {
			if ((max.hasFilter())&&(max.matchFilter(value)))
				return true;
			else if ((min.hasFilter())&&(min.matchFilter(value)))
				return true;
			else if ((eq.hasFilter())&&(eq.matchFilter(value)))
				return true;
			else if ((neq.hasFilter())&&(neq.matchFilter(value)))
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
			} else {
				throw parse_exception("Unknown filter key: " + value);
			}
			return *this;
		}
	};
	typedef filter_all_numeric<unsigned int, checkHolders::time_handler<unsigned int> > filter_all_times;
}