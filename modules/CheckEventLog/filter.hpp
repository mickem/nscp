#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <Windows.h>
#endif

#include <map>
#include <string>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <error.hpp>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "eventlog_record.hpp"

namespace eventlog_filter {

	struct filter_obj : boost::noncopyable {
		const EventLogRecord &record;

		filter_obj(const EventLogRecord &record) : record(record) {}

		long long get_id() {
			return record.eventID(); 
		}
		std::string get_source() {
			return utf8::cvt<std::string>(record.get_source());
		}
		std::string get_computer() {
			return utf8::cvt<std::string>(record.get_computer());
		}
		long long get_el_type() {
			return record.eventType(); 
		}
		long long get_severity() {
			return record.severity();
		}
		std::string get_message() {
			return utf8::cvt<std::string>(record.render_message());
		}
		std::string get_strings() {
			return utf8::cvt<std::string>(record.enumStrings());
		}
		std::string get_log() {
			return utf8::cvt<std::string>(record.get_log());
		}
		long long get_written() {
			return record.written(); 
		}
		long long get_category() {
			return record.category(); 
		}
		long long get_facility() {
			return record.facility(); 
		}
		long long get_customer() {
			return record.customer(); 
		}
		long long get_raw_id() {
			return record.raw_id(); 
		}
		long long get_generated() {
			return record.generated(); 
		}
	};


	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		static const parsers::where::value_type type_custom_severity = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;
 		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}
