#include "StdAfx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"



namespace eventlog_filter {

	using namespace parsers::where;

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
	int convert_type(parsers::where::evaluation_context context, std::string str) {
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
		try {
			return strEx::s::stox<int>(str);
		} catch (const std::exception &e) {
			context->error("Failed to convert: " + str);
			return EVENTLOG_ERROR_TYPE;
		}
	}

	parsers::where::node_type fun_convert_severity(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		return parsers::where::factory::create_int(convert_severity(subject->get_string_value(context)));
	}
	parsers::where::node_type fun_convert_type(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		return parsers::where::factory::create_int(convert_type(context, subject->get_string_value(context)));
	}

	//////////////////////////////////////////////////////////////////////////

	filter_obj_handler::filter_obj_handler() {

		registry_.add_string()
			("source", boost::bind(&filter_obj::get_source, _1), "Source system.")
			("message", boost::bind(&filter_obj::get_message, _1), "The message renderd as a string.")
			("strings", boost::bind(&filter_obj::get_strings, _1), "The message content. Significantly faster than message yet yields similar results.")
			("computer", boost::bind(&filter_obj::get_computer, _1), "Which computer generated the message")
			("log", boost::bind(&filter_obj::get_log, _1), "alias for file")
			("file", boost::bind(&filter_obj::get_log, _1), "The logfile name")
			;

		registry_.add_int()
			("id", boost::bind(&filter_obj::get_id, _1), "Eventlog id")
			("type", type_custom_type, boost::bind(&filter_obj::get_el_type, _1), "alias for level (old)")
			("level", type_custom_type, boost::bind(&filter_obj::get_el_type, _1), "Severity level (error, warning, info, success, auditSucess, auditFailure)")
			("severity", type_custom_severity, boost::bind(&filter_obj::get_severity, _1), "Probably not what you want.This is the technical severity of the mseesage often level is what you are looking for.")
			("generated", type_date, boost::bind(&filter_obj::get_generated, _1), "When the message was generated")
			("written", type_date, boost::bind(&filter_obj::get_written, _1), "When the message was written to file")
			("category", boost::bind(&filter_obj::get_category, _1), "TODO")
			("qualifier", boost::bind(&filter_obj::get_facility, _1), "TODO")
			("facility", boost::bind(&filter_obj::get_facility, _1), "TODO")
			("customer", boost::bind(&filter_obj::get_customer, _1), "TODO")
			("rawid", boost::bind(&filter_obj::get_raw_id, _1), "Raw message id (contains many other fields all baked into a single number)")
			;

		registry_.add_converter()
	 			(type_custom_severity, &fun_convert_severity)
	 			(type_custom_type, &fun_convert_type)
	 			;
	}
}