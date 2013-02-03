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
#include <parsers/filter/modern_filter.hpp>

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
		long long get_column_number(int col) const {
			if (col >= 1 && col <= chunks.size())
				return strEx::s::stox<long long>(chunks[col-1]);
			return 0;
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
		std::wstring get_count_str() const {
			return strEx::itos(count);
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


	struct log_summary : public modern_filter::generic_summary<log_summary> {
		std::string filename;

		std::string get_filename() {
			return filename;
		}

		static boost::function<std::string(log_summary*)> get_function(std::string key) {
			if (key == "file" || key == "filename")
				return &log_summary::get_filename;
			return modern_filter::generic_summary<log_summary>::get_function(key);
		}
		bool add_performance_data_metric(const std::string metric) {
			boost::function<std::string(log_summary*)> f = get_function(metric);
			if (f) {
				metrics[metric] = boost::bind(f,this);
				return true;
			}
			return false;
		}

	};

	typedef modern_filter::modern_filters<logfile_filter::filter_obj, logfile_filter::filter_obj_handler, log_summary> filter;
}
