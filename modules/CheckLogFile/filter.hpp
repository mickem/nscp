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


	struct log_summary {
		long long match_count;
		long long ok_count;
		long long warn_count;
		long long crit_count;
		std::string message;
		std::string error;
		std::string filename;

		log_summary() : match_count(0), ok_count(0), warn_count(0), crit_count(0) {}

		void matched(std::string &line) {
			error = line;
			match_count++;
		}
		void matched_ok(std::string &line) {
			error = line;
			ok_count++;
		}
		void matched_warn(std::string &line) {
			error = line;
			warn_count++;
		}
		void matched_crit(std::string &line) {
			error = line;
			crit_count++;
		}
		std::string get_message() {
			return message;
		}
		std::string get_error() {
			return error;
		}
		std::string get_match_count() {
			return strEx::s::xtos(match_count);
		}
		std::string get_ok_count() {
			return strEx::s::xtos(ok_count);
		}
		std::string get_warn_count() {
			return strEx::s::xtos(warn_count);
		}
		std::string get_crit_count() {
			return strEx::s::xtos(crit_count);
		}
		std::string get_filename() {
			return filename;
		}
		static boost::function<std::string(log_summary*)> get_function(std::string key) {
			if (key == "file" || key == "filename")
				return &log_summary::get_filename;
			if (key == "messages" || key == "lines")
				return &log_summary::get_message;
			if (key == "error" || key == "last")
				return &log_summary::get_error;
			if (key == "count" || key == "match_count")
				return &log_summary::get_match_count;
			if (key == "ok_count")
				return &log_summary::get_ok_count;
			if (key == "warn_count" || key == "warning_count" || key == "warnings")
				return &log_summary::get_warn_count;
			if (key == "crit_count" || key == "critical_count" || key == "criticals")
				return &log_summary::get_crit_count;
			return boost::function<std::string(log_summary*)>();
		}
	};

	typedef modern_filter::modern_filters<logfile_filter::filter_obj, logfile_filter::filter_obj_handler, log_summary> filter;
}
