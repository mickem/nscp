#pragma once

#include <map>
#include <string>

#include <parsers/where_parser.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <error.hpp>

#include <parsers/where/expression_ast.hpp>
#include <parsers/where/varible_handler.hpp>

#include <parsers/filter/where_filter.hpp>

#include "file_info.hpp"


namespace file_filter {
	struct filter_obj_handler;
	struct filter_obj {
		typedef parsers::where::expression_ast<filter_obj_handler> ast_expr_type;


		filter_obj() 
			: ullCreationTime(0)
			, ullLastAccessTime(0)
			, ullLastWriteTime(0)
			, ullSize(0)
			, ullNow(0)
		{}
		filter_obj(std::wstring path_, std::wstring filename_, __int64 now = 0, __int64 creationTime = 0, __int64 lastAccessTime = 0, __int64 lastWriteTime = 0, __int64 size = 0, DWORD attributes = 0) 
			: path(path_)
			, filename(filename_)
			, ullCreationTime(creationTime)
			, ullLastAccessTime(lastAccessTime)
			, ullLastWriteTime(lastWriteTime)
			, ullSize(size)
			, ullNow(now)
			, attributes(attributes)
		{};

		static filter_obj get(unsigned long long now, const WIN32_FILE_ATTRIBUTE_DATA info, std::wstring path, std::wstring filename);
		static filter_obj get(unsigned long long now, const BY_HANDLE_FILE_INFORMATION info, std::wstring path, std::wstring filename);
		static filter_obj get(unsigned long long now, const WIN32_FIND_DATA info, std::wstring path);
		static filter_obj get(unsigned long long now, std::wstring path, std::wstring filename);
		static filter_obj get(unsigned long long now, std::wstring file);
		static filter_obj get(std::wstring file);

		//filter_obj(file_info &record);
		std::wstring get_filename() { return filename; }
		std::wstring get_path() { return path; }
		//std::wstring get_version();
		//long long get_line_count();
		//long long get_access();
		//long long get_creation();
		//long long get_write();

		__int64 get_creation() {
			std::cout << ullNow << " ? " << ullCreationTime << " => " << ullNow-ullCreationTime << " (" << (ullNow-ullCreationTime)/MSECS_TO_100NS << ")" << std::endl;
			return (ullNow-ullCreationTime)/MSECS_TO_100NS;
		}
		__int64 get_access() {
			return (ullNow-ullLastAccessTime)/MSECS_TO_100NS;
		}
		__int64 get_write() {
			return (ullNow-ullLastWriteTime)/MSECS_TO_100NS;
		}
		unsigned long long get_size() { return ullSize; }
		std::wstring render(std::wstring syntax, std::wstring datesyntax);
		std::wstring get_version();
		unsigned long get_line_count();


	public:
		void error(std::wstring err) { errors.push_back(err); }
		bool has_error() { return !errors.empty(); }
		std::wstring get_error() { return strEx::joinEx(errors, _T(", ")); }

	private:
		std::list<std::wstring> errors;
		//file_info &record;


	public:

		filter_obj( const filter_obj& other) 
			: ullSize(other.ullSize)
			, ullCreationTime(other.ullCreationTime)
			, ullLastAccessTime(other.ullLastAccessTime)
			, ullLastWriteTime(other.ullLastWriteTime)
			, ullNow(other.ullNow)
			, filename(other.filename)
			, path(other.path)
			, cached_version(other.cached_version)
			, cached_count(other.cached_count)
			, attributes(other.attributes)
		{}

		const filter_obj& operator=( const filter_obj&other ) {
			ullSize = other.ullSize;
			ullCreationTime = other.ullCreationTime;
			ullLastAccessTime = other.ullLastAccessTime;
			ullLastWriteTime = other.ullLastWriteTime;
			ullNow = other.ullNow;
			filename = other.filename;
			path = other.path;
			cached_version = other.cached_version;
			cached_count = other.cached_count;
			attributes = other.attributes;
		}


		unsigned long long ullSize;
		__int64 ullCreationTime;
		__int64 ullLastAccessTime;
		__int64 ullLastWriteTime;
		__int64 ullNow;
		std::wstring filename;
		std::wstring path;
		boost::optional<std::wstring> cached_version;
		boost::optional<unsigned long> cached_count;
		DWORD attributes;

		static const __int64 MSECS_TO_100NS = 10000;

	};

	typedef filter_obj flyweight_type;
	struct filter_obj_handler : public parsers::where::varible_handler<filter_obj_handler, filter_obj> {

		static const parsers::where::value_type type_custom_severity = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

		typedef parsers::where::varible_handler<filter_obj_handler, filter_obj> handler;
		typedef parsers::where::expression_ast<filter_obj_handler> ast_expr_type;
		typedef std::map<std::wstring,parsers::where::value_type> types_type;
		typedef std::list<std::wstring> error_type;
		typedef filter_obj object_type;

		filter_obj_handler();

		handler::bound_string_type bind_string(std::wstring key);
		handler::bound_int_type bind_int(std::wstring key);
		bool has_function(parsers::where::value_type to, std::wstring name, ast_expr_type subject);
		handler::bound_function_type bind_function(parsers::where::value_type to, std::wstring name, ast_expr_type subject);

		bool has_variable(std::wstring key);
		parsers::where::value_type get_type(std::wstring key);
		bool can_convert(parsers::where::value_type from, parsers::where::value_type to);

		flyweight_type static_record;
		object_type get_static_object() {
			return object_type(static_record);
		}

	public:
		void error(std::wstring err) { errors.push_back(err); }
		bool has_error() { return !errors.empty(); }
		std::wstring get_error() { return strEx::joinEx(errors, _T(", ")); }
	private:
		std::list<std::wstring> errors;

	private:
		types_type types;

	};


	struct file_finder_data_arguments : public where_filter::argument_interface<flyweight_type> {

		typedef where_filter::argument_interface<flyweight_type> parent_type;
		enum filter_types {
			filter_plus = 1,
			filter_minus = 2,
			filter_normal = 3
		};
		bool bFilterAll;
		bool bFilterIn;

		int max_level;
		int debugThreshold;

		bool bShowDescriptions;
		std::wstring pattern;
		unsigned long long now;

		std::list<file_finder::filter> filter_chain;

		file_finder_data_arguments(std::wstring pattern, int max_depth, parent_type::error_type error, std::wstring syntax, std::wstring datesyntax, bool debug = false);


		bool is_valid_level(int current_level) {
			return (max_level == -1) || (current_level <= max_level);
		}

		typedef std::pair<int,file_finder::filter> filteritem_type;
		typedef std::list<filteritem_type > filterlist_type;

		filterlist_type old_chain;

	};
	typedef where_filter::engine_interface<flyweight_type> filter_engine_type;
	//typedef where_filter::argument_interface<flyweight_type> filter_argument_type;
	typedef file_finder_data_arguments filter_argument_type;
	typedef where_filter::result_counter_interface<flyweight_type> filter_result_type;

	struct filesize_engine_interface_type : public filter_engine_type {
		virtual unsigned long long get_size() = 0;
	};


	typedef boost::shared_ptr<filter_engine_type> filter_engine;
	typedef boost::shared_ptr<filesize_engine_interface_type> filesize_engine_interface;
	typedef boost::shared_ptr<filter_argument_type> filter_argument;
	typedef boost::shared_ptr<filter_result_type> filter_result;

	struct factories {
		static filter_engine create_engine(filter_argument arg);
		static filter_engine create_old_engine(filter_argument arg);
		static filesize_engine_interface create_size_engine();
		static filter_result create_result(filter_argument arg);
		static filter_argument create_argument(std::wstring pattern, int max_depth, std::wstring syntax, std::wstring datesyntax);
	};


/*
	struct where_mode_filter : public file_finder::file_engine_interface {
		file_finder::file_finder_arguments &data;
		std::string message;
		parsers::where::parser<file_filter_obj_handler> ast_parser;
		file_filter_obj_handler object_handler;

		where_mode_filter(file_finder::file_finder_arguments &data) : data(data) {}
		bool boot() {return true; }

		bool validate(std::wstring &message);
		bool match(file_info &record);
		std::wstring get_name() {
			return _T("where");
		}
		std::wstring get_subject() { return data.filter; }
	};
*/


}
