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

#include <buffer.hpp>
#include <handle.hpp>

#include "eventlog_record.hpp"
#include "modern_eventlog.hpp"

namespace eventlog_filter {


	struct filter_obj  {
		virtual long long get_id() = 0;
		virtual std::string get_source() = 0;
		virtual std::string get_computer() = 0;
		virtual long long get_el_type() = 0;
		virtual std::string get_el_type_s() = 0;
		virtual long long get_severity() = 0;
		virtual std::string get_message() = 0;
		virtual std::string get_strings() = 0;
		virtual std::string get_log() = 0;
		virtual long long get_written() = 0;
		virtual long long get_category() = 0;
		virtual long long get_facility() = 0;
		virtual long long get_customer() = 0;
		virtual long long get_raw_id() = 0;
		virtual long long get_generated() = 0;
		virtual bool is_modern() = 0;
	};


	struct old_filter_obj : filter_obj {
		const EventLogRecord &record;
		const int truncate_message;

		old_filter_obj(const EventLogRecord &record, const int truncate_message) : record(record), truncate_message(truncate_message) {}

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
		std::string get_el_type_s();
		long long get_severity() {
			return record.severity();
		}
		std::string get_message() {
			return utf8::cvt<std::string>(record.render_message(truncate_message));
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
		bool is_modern() { return false; }
	};

	struct new_filter_obj : filter_obj {
		const std::string &logfile;
		eventlog::evt_handle &hEvent;
		hlp::buffer<wchar_t, eventlog::api::PEVT_VARIANT> buffer;
		const int truncate_message;

		new_filter_obj(const std::string &logfile, eventlog::evt_handle &hEvent, eventlog::evt_handle &hContext, const int truncate_message);

		long long get_id() {
			return buffer.get()[eventlog::api::EvtSystemEventID].UInt16Val;
		}
		std::string get_source();
		std::string get_computer();
		long long get_el_type();
		std::string get_el_type_s();
		long long get_severity() {
			return 0;
		}
		std::string get_message();
		std::string get_strings() {
			return get_message();
		}
		std::string get_log();
		long long get_written();
		long long get_category();
		long long get_facility() {
			return 0; 
		}
		long long get_customer() {
			return 0; 
		}
		long long get_raw_id() {
			return 0; 
		}
		long long get_generated() {
			return 0; 
		}
		bool is_modern() { return true; }
	};


	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		static const parsers::where::value_type type_custom_severity = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;
 		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}
