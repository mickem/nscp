#include "stdafx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"

using namespace parsers::where;

node_type get_column_fun(const value_type, evaluation_context context, const node_type subject) {
	std::list<node_type> l = subject->get_list_value(context);
	if (l.size() != 1) {
		context->error("Invalid number of arguments for function");
		return factory::create_false();
	}
	node_type f = l.front();
	long long idx = f->get_int_value(context);
	logfile_filter::native_context* n_context = reinterpret_cast<logfile_filter::native_context*>(context.get());
	std::string value = n_context->get_object()->get_column(idx);
	return factory::create_string(value);
}


//////////////////////////////////////////////////////////////////////////

logfile_filter::filter_obj_handler::filter_obj_handler() {

	registry_.add_string()
		("line", boost::bind(&filter_obj::get_line, _1), "Match the content of an entire line")
		("column1", boost::bind(&filter_obj::get_column, _1, 1), boost::bind(&filter_obj::get_column_number, _1, 1), "The value in the first column")
		("column2", boost::bind(&filter_obj::get_column, _1, 2), boost::bind(&filter_obj::get_column_number, _1, 2), "The value in the second column")
		("column3", boost::bind(&filter_obj::get_column, _1, 3), boost::bind(&filter_obj::get_column_number, _1, 3), "The value in the third column")
		("column4", boost::bind(&filter_obj::get_column, _1, 4), boost::bind(&filter_obj::get_column_number, _1, 4), "The value in the 4:th column")
		("column5", boost::bind(&filter_obj::get_column, _1, 5), boost::bind(&filter_obj::get_column_number, _1, 5), "The value in the 5:th column")
		("column6", boost::bind(&filter_obj::get_column, _1, 6), boost::bind(&filter_obj::get_column_number, _1, 6), "The value in the 6:th column")
		("column7", boost::bind(&filter_obj::get_column, _1, 7), boost::bind(&filter_obj::get_column_number, _1, 7), "The value in the 7:th column")
		("column8", boost::bind(&filter_obj::get_column, _1, 8), boost::bind(&filter_obj::get_column_number, _1, 8), "The value in the 8:th column")
		("column9", boost::bind(&filter_obj::get_column, _1, 9), boost::bind(&filter_obj::get_column_number, _1, 9), "The value in the 9:th column")
		("filename", boost::bind(&filter_obj::filename, _1), "The name of the file")
		("file", boost::bind(&filter_obj::filename, _1), "The name of the file")
		;

 	registry_.add_string_fun()
 		("column", &get_column_fun, "Fetch the value from the given column number.\nSyntax: column(<coulmn number>)")
 		;
}
 