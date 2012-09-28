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

std::wstring logfile_filter::filter_obj::render(std::wstring syntax) {
	return _T("TODO");
}

logfile_filter::filter_obj::expression_ast_type logfile_filter::filter_obj::get_column_fun(parsers::where::value_type target_type, parsers::where::filter_handler handler, const expression_ast_type *subject) {
	expression_ast::list_type l = subject->get_list();
	if (l.size() != 1) {
		return expression_ast_type(parsers::where::string_value(_T("ERROR")));
	}
	expression_ast_type f = l.front();
	int idx  = f.get_int(handler);
	return expression_ast_type(parsers::where::string_value(get_column(idx)));
}


//////////////////////////////////////////////////////////////////////////



logfile_filter::filter_obj_handler::filter_obj_handler() {
	using namespace boost::assign;
	using namespace parsers::where;
	insert(types)
		(_T("line"), type_string)
		(_T("count"), type_int)
		(_T("column1"), type_string)
		(_T("column2"), type_string)
		(_T("column3"), type_string)
		(_T("column4"), type_string)
		(_T("column5"), type_string)
		(_T("column6"), type_string)
		(_T("column7"), type_string)
		(_T("column8"), type_string)
		(_T("column9"), type_string)
		(_T("filename"),type_string)
		(_T("file"),type_string)
		;
}

bool logfile_filter::filter_obj_handler::has_variable(std::wstring key) {
	return types.find(key) != types.end();
}
parsers::where::value_type logfile_filter::filter_obj_handler::get_type(std::wstring key) {
	types_type::const_iterator cit = types.find(key);
	if (cit == types.end())
		return parsers::where::type_invalid;
	return cit->second;
}
bool logfile_filter::filter_obj_handler::can_convert(parsers::where::value_type from, parsers::where::value_type to) {
	return false;
}
logfile_filter::filter_obj_handler::base_handler::bound_string_type logfile_filter::filter_obj_handler::bind_simple_string(std::wstring key) {
	base_handler::bound_string_type ret;
	if (key == _T("column1"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 1);
	if (key == _T("column2"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 2);
	if (key == _T("column3"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 3);
	if (key == _T("column4"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 4);
	if (key == _T("column5"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 5);
	if (key == _T("column6"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 6);
	if (key == _T("column7"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 7);
	if (key == _T("column8"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 8);
	if (key == _T("column9"))
		ret = BOOST_BIND(&filter_obj::get_column, _1, 9);
	if (key == _T("line"))
		ret = &filter_obj::get_line;
	if (key == _T("file") || key == _T("filename"))
		ret = &filter_obj::get_filename;
	return ret;
}
logfile_filter::filter_obj_handler::base_handler::bound_int_type logfile_filter::filter_obj_handler::bind_simple_int(std::wstring key) {
	base_handler::bound_int_type ret;
	if (key == _T("count"))
		ret = &filter_obj::get_count;
	return ret;
}

bool logfile_filter::filter_obj_handler::has_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject) {
	std::wcout << name << std::endl;
	if (to == type_string && name == _T("column"))
		return true;
	return false;
}
logfile_filter::filter_obj_handler::base_handler::bound_function_type logfile_filter::filter_obj_handler::bind_simple_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject) {
	std::wcout << name << std::endl;
	base_handler::bound_function_type ret;
	if (to == type_string && name == _T("column")) {
		ret = &filter_obj::get_column_fun;
	}
	return ret;
}

//////////////////////////////////////////////////////////////////////////

logfile_filter::filter_engine logfile_filter::factories::create_engine(logfile_filter::filter_argument arg, std::string filter) {
	arg->filter = utf8::cvt<std::wstring>(filter);
	return filter_engine(new filter_engine_type(arg));
}
logfile_filter::filter_argument logfile_filter::factories::create_argument(std::wstring syntax, std::wstring datesyntax) {
	return filter_argument(new logfile_filter::filter_argument_type(logfile_filter::filter_argument_type::error_type(new where_filter::nsc_error_handler()), syntax, datesyntax));
}

logfile_filter::filter_result logfile_filter::factories::create_result(logfile_filter::filter_argument arg) {
	return filter_result(new where_filter::simple_count_result<filter_obj>(arg));
}





