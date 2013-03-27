#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <Windows.h>
#endif

#include <map>
#include <string>

#include <parsers/where.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <error.hpp>

#include <parsers/where/expression_ast.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/filter/where_filter.hpp>
#include <parsers/filter/where_filter_impl.hpp>

#include "eventlog_record.hpp"

namespace eventlog_filter {

	struct filter_obj_handler;
	struct filter_obj {
		typedef parsers::where::expression_ast expression_ast_type;

		EventLogRecord &record;
		filter_obj(EventLogRecord &record) : record(record) {}

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

		expression_ast_type fun_convert_severity(parsers::where::value_type target_type, parsers::where::filter_handler handler, const expression_ast_type *subject);
		expression_ast_type fun_convert_type(parsers::where::value_type target_type, parsers::where::filter_handler handler, const expression_ast_type *subject);

		int convert_severity(std::string str) {
			if (str == "success" || str == "ok")
				return 0;
			if (str == "informational" || str == "info")
				return 1;
			if (str == "warning" || str == "warn")
				return 2;
			if (str == "error" || str == "err")
				return 3;
			return strEx::s::stox<int>(str);
		}
		int convert_type(std::string str) {
			if (str == "error")
				return EVENTLOG_ERROR_TYPE;
			if (str == "warning")
				return EVENTLOG_WARNING_TYPE;
			if (str == "info")
				return EVENTLOG_INFORMATION_TYPE;
			if (str == "success")
				return EVENTLOG_SUCCESS;
			if (str == "auditSuccess")
				return EVENTLOG_AUDIT_SUCCESS;
			if (str == "auditFailure")
				return EVENTLOG_AUDIT_FAILURE;
			return strEx::s::stox<int>(str);
		}
		std::string render(const std::string syntax, const std::string datesyntax);
	};



	struct filter_obj_handler : public parsers::where::filter_handler_impl<filter_obj> {

		typedef filter_obj object_type;
		typedef boost::shared_ptr<object_type> object_instance_type;
		typedef parsers::where::filter_handler_impl<object_type> base_handler;

		typedef std::map<std::string,parsers::where::value_type> types_type;
		typedef parsers::where::expression_ast expression_ast_type;


		filter_obj_handler();
		bool has_variable(std::string key);
		parsers::where::value_type get_type(std::string key);
		bool can_convert(parsers::where::value_type from, parsers::where::value_type to);
		base_handler::bound_string_type bind_simple_string(std::string key);
		base_handler::bound_int_type bind_simple_int(std::string key);
		bool has_function(parsers::where::value_type to, std::string name, expression_ast_type *subject);
		base_handler::bound_function_type bind_simple_function(parsers::where::value_type to, std::string name, expression_ast_type *subject);
	private:
		types_type types;
		static const parsers::where::value_type type_custom_severity = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

	};


	struct data_arguments : public where_filter::argument_interface {

		typedef where_filter::argument_interface parent_type;
		//bool bFilterAll;
		//bool bFilterIn;
		bool bShowDescriptions;
		unsigned long long now;
		std::string alias;

		data_arguments(parent_type::error_type error, std::string syntax, std::string datesyntax, bool debug = false) : where_filter::argument_interface(error, syntax, datesyntax)
		{}

	};

	typedef data_arguments filter_argument_type;
	typedef where_filter::engine_impl<filter_obj, filter_obj_handler, boost::shared_ptr<filter_argument_type> > filter_engine_type;
	typedef where_filter::result_counter_interface<filter_obj> filter_result_type;

	typedef boost::shared_ptr<filter_engine_type> filter_engine;
	typedef boost::shared_ptr<filter_argument_type> filter_argument;
	typedef boost::shared_ptr<filter_result_type> filter_result;

	struct factories {
		static filter_engine create_engine(filter_argument arg);
		static filter_result create_result(filter_argument arg);
		static filter_argument create_argument(std::string syntax, std::string datesyntax);
	};
}
