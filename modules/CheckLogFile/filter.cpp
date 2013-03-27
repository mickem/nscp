#include "stdafx.h"

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

using namespace boost::assign;
using namespace parsers::where;

logfile_filter::filter_obj::expression_ast_type logfile_filter::filter_obj::get_column_fun(parsers::where::value_type target_type, parsers::where::filter_handler handler, const expression_ast_type *subject) {
	expression_ast::list_type l = subject->get_list();
	if (l.size() != 1) {
		return expression_ast_type(parsers::where::string_value("ERROR"));
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
		("line", type_string)
		("count", type_int)
		("column1", type_string)
		("column2", type_string)
		("column3", type_string)
		("column4", type_string)
		("column5", type_string)
		("column6", type_string)
		("column7", type_string)
		("column8", type_string)
		("column9", type_string)
		("column_int1", type_int)
		("column_int2", type_int)
		("column_int3", type_int)
		("column_int4", type_int)
		("column_int5", type_int)
		("column_int6", type_int)
		("column_int7", type_int)
		("column_int8", type_int)
		("column_int9", type_int)
		("filename",type_string)
		("file",type_string)
		;
}

bool logfile_filter::filter_obj_handler::has_variable(std::string key) {
	return types.find(key) != types.end();
}
parsers::where::value_type logfile_filter::filter_obj_handler::get_type(std::string key) {
	types_type::const_iterator cit = types.find(key);
	if (cit == types.end())
		return parsers::where::type_invalid;
	return cit->second;
}
bool logfile_filter::filter_obj_handler::can_convert(parsers::where::value_type from, parsers::where::value_type to) {
	return false;
}
logfile_filter::filter_obj_handler::base_handler::bound_string_type logfile_filter::filter_obj_handler::bind_simple_string(std::string key) {
	base_handler::bound_string_type ret;
	if (key.length() > 6 && key.substr(0,6) == "column") {
		std::string index = key.substr(6);
		if (index.find_first_not_of("0123456789") == std::string::npos) {
			ret = boost::bind(&filter_obj::get_column, _1, strEx::s::stox<int>(index));
		}
	}
	if (key == "line")
		ret = &filter_obj::get_line;
	if (key == "file" || key == "filename")
		ret = &filter_obj::get_filename;
	if (key == "count")
		ret = &filter_obj::get_count_str;
	return ret;
}
logfile_filter::filter_obj_handler::base_handler::bound_int_type logfile_filter::filter_obj_handler::bind_simple_int(std::string key) {
	base_handler::bound_int_type ret;
	if (key.length() > 6 && key.substr(0,10) == "column_int") {
		std::string index = key.substr(10);
		if (index.find_first_not_of("0123456789") == std::string::npos) {
			ret = BOOST_BIND(&filter_obj::get_column_number, _1, strEx::s::stox<int>(index));
		}
	}
	if (key == "count")
		ret = &filter_obj::get_count;
	return ret;
}

bool logfile_filter::filter_obj_handler::has_function(parsers::where::value_type to, std::string name, expression_ast_type *subject) {
	if (to == type_string && name == "column")
		return true;
	return false;
}
logfile_filter::filter_obj_handler::base_handler::bound_function_type logfile_filter::filter_obj_handler::bind_simple_function(parsers::where::value_type to, std::string name, expression_ast_type *subject) {
	base_handler::bound_function_type ret;
	if (to == type_string && name == "column") {
		ret = &filter_obj::get_column_fun;
	}
	return ret;
}

//////////////////////////////////////////////////////////////////////////
