#pragma once

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

namespace logfile_filter {

	struct filter_obj_handler;
	struct filter_obj {
		std::string filename;
		std::string line;
		std::vector<std::string> chunks;
		long long count;
		typedef parsers::where::expression_ast expression_ast_type;
		filter_obj(std::string filename, std::string line, std::list<std::string> chunks, long long count) : filename(filename), line(line), chunks(chunks.begin(), chunks.end()), count(count) {}

		std::wstring get_column(int col) const {
			if (col >= 1 && col <= chunks.size())
				return utf8::cvt<std::wstring>(chunks[col-1]);
			return _T("");
		}
		std::wstring get_filename() const {
			return utf8::cvt<std::wstring>(filename);
		}
		std::wstring get_line() const {
			return utf8::cvt<std::wstring>(line);
		}
		long long get_count() const {
			return count;
		}
		void matched() {
			count++;
		}
		expression_ast_type get_column_fun(parsers::where::value_type target_type, parsers::where::filter_handler handler, const expression_ast_type *subject);
		std::wstring render(std::wstring syntax);
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
		static filter_engine create_engine(filter_argument arg, std::string filter);
		static filter_result create_result(filter_argument arg);
		static filter_argument create_argument(std::wstring syntax, std::wstring datesyntax);
	};
}
