#pragma once

#ifdef WIN32
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
		std::wstring get_source() {
			return record.get_source(); 
		}
		std::wstring get_computer() {
			return record.get_computer(); 
		}
		long long get_el_type() {
			return record.eventType(); 
		}
		long long get_severity() {
			return record.severity();
		}
		std::wstring get_message() {
			return record.render_message(); 
		}
		std::wstring get_strings() {
			return record.enumStrings(); 
		}
		std::wstring get_log() {
			return record.get_log(); 
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

		int convert_severity(std::wstring str) {
			if (str == _T("success") || str == _T("ok"))
				return 0;
			if (str == _T("informational") || str == _T("info"))
				return 1;
			if (str == _T("warning") || str == _T("warn"))
				return 2;
			if (str == _T("error") || str == _T("err"))
				return 3;
			return strEx::stoi(str);
		}
		int convert_type(std::wstring str) {
			if (str == _T("error"))
				return EVENTLOG_ERROR_TYPE;
			if (str == _T("warning"))
				return EVENTLOG_WARNING_TYPE;
			if (str == _T("info"))
				return EVENTLOG_INFORMATION_TYPE;
			if (str == _T("success"))
				return EVENTLOG_SUCCESS;
			if (str == _T("auditSuccess"))
				return EVENTLOG_AUDIT_SUCCESS;
			if (str == _T("auditFailure"))
				return EVENTLOG_AUDIT_FAILURE;
			return strEx::stoi(str);
		}
		std::wstring render(std::wstring syntax, std::wstring datesyntax);

	};



	struct filter_obj_handler : public parsers::where::filter_handler_impl<filter_obj> {

		typedef filter_obj object_type;
		typedef boost::shared_ptr<object_type> object_instance_type;
		typedef parsers::where::filter_handler_impl<object_type> base_handler;

		typedef std::map<std::wstring,parsers::where::value_type> types_type;
		typedef parsers::where::expression_ast expression_ast_type;


		filter_obj_handler();
		bool has_variable(std::wstring key);
		parsers::where::value_type get_type(std::wstring key);
		bool can_convert(parsers::where::value_type from, parsers::where::value_type to);
		base_handler::bound_string_type bind_simple_string(std::wstring key);
		base_handler::bound_int_type bind_simple_int(std::wstring key);
		bool has_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject);
		base_handler::bound_function_type bind_simple_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject);
	private:
		types_type types;
		static const parsers::where::value_type type_custom_severity = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

	};


	struct data_arguments : public where_filter::argument_interface {

		typedef where_filter::argument_interface parent_type;
		bool bFilterAll;
		bool bFilterIn;
		bool bShowDescriptions;
		unsigned long long now;
		std::wstring alias;

		data_arguments(parent_type::error_type error, std::wstring syntax, std::wstring datesyntax, bool debug = false) : where_filter::argument_interface(error, syntax, datesyntax)
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
		static filter_argument create_argument(std::wstring syntax, std::wstring datesyntax);
	};
}
