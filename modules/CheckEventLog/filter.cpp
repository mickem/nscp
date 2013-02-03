#include "StdAfx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>
#include <parsers/filter/where_filter.hpp>
#include <parsers/filter/where_filter_impl.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"

//#include <config.h>

using namespace boost::assign;
using namespace parsers::where;

eventlog_filter::filter_obj::expression_ast_type eventlog_filter::filter_obj::fun_convert_severity(parsers::where::value_type target_type, parsers::where::filter_handler handler, const expression_ast_type *subject) {
	return expression_ast_type(parsers::where::int_value(convert_severity(subject->get_string(handler))));
}
eventlog_filter::filter_obj::expression_ast_type eventlog_filter::filter_obj::fun_convert_type(parsers::where::value_type target_type, parsers::where::filter_handler handler, const expression_ast_type *subject) {
	return expression_ast_type(parsers::where::int_value(convert_type(subject->get_string(handler))));
}

std::wstring eventlog_filter::filter_obj::render(std::wstring syntax, std::wstring datesyntax) {
	return record.render(true, syntax, datesyntax);
}


//////////////////////////////////////////////////////////////////////////



eventlog_filter::filter_obj_handler::filter_obj_handler() {
	using namespace boost::assign;
	using namespace parsers::where;
	insert(types)
		(_T("id"), (type_int))
		(_T("source"), (type_string))
		(_T("file"), (type_string))
		(_T("log"), (type_string))
		(_T("type"), (type_custom_type))
		(_T("level"), (type_custom_type))
		(_T("severity"), (type_custom_severity))
		(_T("category"), (type_int))
		(_T("qualifier"), (type_int))
		(_T("facility"), (type_int))
		(_T("customer"), (type_int))
		(_T("rawid"), (type_int))
		(_T("message"), (type_string))
		(_T("strings"), (type_string))
		(_T("computer"), (type_string))
		(_T("written"), (type_date))
		(_T("generated"), (type_date));
}

bool eventlog_filter::filter_obj_handler::has_variable(std::wstring key) {
	return types.find(key) != types.end();
}
parsers::where::value_type eventlog_filter::filter_obj_handler::get_type(std::wstring key) {
	types_type::const_iterator cit = types.find(key);
	if (cit == types.end())
		return parsers::where::type_invalid;
	return cit->second;
}
bool eventlog_filter::filter_obj_handler::can_convert(parsers::where::value_type from, parsers::where::value_type to) {
	if ((from == parsers::where::type_string)&&(to == type_custom_severity))
		return true;
	if ((from == parsers::where::type_string)&&(to == type_custom_type))
		return true;
	return false;
}
eventlog_filter::filter_obj_handler::base_handler::bound_string_type eventlog_filter::filter_obj_handler::bind_simple_string(std::wstring key) {
	base_handler::bound_string_type ret;
	if (key == _T("source"))
		ret = &filter_obj::get_source;
	else if (key == _T("message"))
		ret = &filter_obj::get_message;
	else if (key == _T("strings"))
		ret = &filter_obj::get_strings;
	else if (key == _T("computer"))
		ret = &filter_obj::get_computer;
	else if (key == _T("log"))
		ret = &filter_obj::get_log;
	else if (key == _T("file"))
		ret = &filter_obj::get_log;
	else
		NSC_DEBUG_MSG_STD(_T("Failed to bind (string): ") + key);
	return ret;
}
eventlog_filter::filter_obj_handler::base_handler::bound_int_type eventlog_filter::filter_obj_handler::bind_simple_int(std::wstring key) {
	base_handler::bound_int_type ret;
	if (key == _T("id"))
		ret = &filter_obj::get_id;
	else if (key == _T("type"))
		ret = &filter_obj::get_el_type;
	else if (key == _T("level"))
		ret = &filter_obj::get_el_type;
	else if (key == _T("severity"))
		ret = &filter_obj::get_severity;
	else if (key == _T("generated"))
		ret = &filter_obj::get_generated;
	else if (key == _T("written"))
		ret = &filter_obj::get_written;
	else if (key == _T("category"))
		ret = &filter_obj::get_category;
	else if (key == _T("qualifier"))
		ret = &filter_obj::get_facility;
	else if (key == _T("facility"))
		ret = &filter_obj::get_facility;
	else if (key == _T("customer"))
		ret = &filter_obj::get_customer;
	else if (key == _T("rawid"))
		ret = &filter_obj::get_raw_id;
	else
		NSC_DEBUG_MSG_STD(_T("Failed to bind (int): ") + key);
	return ret;
}

bool eventlog_filter::filter_obj_handler::has_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject) {
	if (to == type_custom_severity)
		return true;
	if (to == type_custom_type)
		return true;
	return false;
}
eventlog_filter::filter_obj_handler::base_handler::bound_function_type eventlog_filter::filter_obj_handler::bind_simple_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject) {
	base_handler::bound_function_type ret;
	if (to == type_custom_severity)
		ret = &filter_obj::fun_convert_severity;
	if (to == type_custom_type)
		ret = &filter_obj::fun_convert_type;
	return ret;
}

//////////////////////////////////////////////////////////////////////////

eventlog_filter::filter_engine eventlog_filter::factories::create_engine(eventlog_filter::filter_argument arg) {
	return filter_engine(new filter_engine_type(arg));
}
eventlog_filter::filter_argument eventlog_filter::factories::create_argument(std::wstring syntax, std::wstring datesyntax) {
	return filter_argument(new eventlog_filter::filter_argument_type(eventlog_filter::filter_argument_type::error_type(new where_filter::nsc_error_handler(GET_CORE())), syntax, datesyntax));
}

eventlog_filter::filter_result eventlog_filter::factories::create_result(eventlog_filter::filter_argument arg) {
	return filter_result(new where_filter::simple_count_result<filter_obj>(arg));
}





