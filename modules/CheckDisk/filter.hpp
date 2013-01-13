#pragma once

#ifdef WIN32
#include <Windows.h>
#endif

#include <map>
#include <string>

#include <parsers/where.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <error.hpp>

#include <parsers/where/expression_ast.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/filter/where_filter.hpp>

#include "file_info.hpp"


namespace file_filter {

	struct filter_obj_handler;
	struct file_object_exception : public std::exception {
		file_object_exception(std::string what) : std::exception(what.c_str()) {}

	};
	struct filter_obj {
		filter_obj() 
			: ullCreationTime(0)
			, ullLastAccessTime(0)
			, ullLastWriteTime(0)
			, ullSize(0)
			, ullNow(0)
		{}
		filter_obj(boost::filesystem::path path_, std::wstring filename_, __int64 now = 0, __int64 creationTime = 0, __int64 lastAccessTime = 0, __int64 lastWriteTime = 0, __int64 size = 0, DWORD attributes = 0) 
			: path(path_)
			, filename(filename_)
			, ullCreationTime(creationTime)
			, ullLastAccessTime(lastAccessTime)
			, ullLastWriteTime(lastWriteTime)
			, ullSize(size)
			, ullNow(now)
			, attributes(attributes)
		{};

#ifdef WIN32
		static filter_obj get(unsigned long long now, const WIN32_FILE_ATTRIBUTE_DATA info, boost::filesystem:: path path, std::wstring filename);
		static filter_obj get(unsigned long long now, const BY_HANDLE_FILE_INFORMATION info, boost::filesystem::path path, std::wstring filename);
		static boost::shared_ptr<filter_obj>  get(unsigned long long now, const WIN32_FIND_DATA info, boost::filesystem::path path);
#endif
		static filter_obj get(unsigned long long now, boost::filesystem::path path, std::wstring filename);
		static filter_obj get(unsigned long long now, std::wstring file);
		static filter_obj get(std::wstring file);

		std::wstring get_filename() { return filename; }
		std::wstring get_path() { return path.wstring(); }

		long long get_creation() {
			return strEx::filetime_to_time(ullCreationTime);
		}
		long long get_access() {
			return strEx::filetime_to_time(ullLastAccessTime);
		}
		long long get_write() {
			return strEx::filetime_to_time(ullLastWriteTime);
		}
		unsigned long long get_size() { return ullSize; }
		std::wstring render(std::wstring syntax, std::wstring datesyntax);
		std::wstring get_version(filter_obj_handler *handler);
		unsigned long get_line_count();

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
		boost::filesystem::path path;
		boost::optional<std::wstring> cached_version;
		boost::optional<unsigned long> cached_count;
		DWORD attributes;

	};

	struct filter_obj_handler : public parsers::where::filter_handler_impl<filter_obj> {
		typedef filter_obj object_type;
		typedef boost::shared_ptr<filter_obj> object_instance_type;
		typedef parsers::where::filter_handler_impl<object_type> base_handler;

		static const parsers::where::value_type type_custom_severity = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

		typedef parsers::where::filter_handler handler;
		typedef parsers::where::expression_ast ast_expr_type;
		typedef std::map<std::wstring,parsers::where::value_type> types_type;
		typedef std::list<std::wstring> error_type;

		filter_obj_handler();

		base_handler::bound_string_type bind_simple_string(std::wstring key);
		base_handler::bound_int_type bind_simple_int(std::wstring key);
		base_handler::bound_function_type bind_simple_function(parsers::where::value_type to, std::wstring name, ast_expr_type *subject);
		bool has_function(parsers::where::value_type to, std::wstring name, ast_expr_type *subject);

		bool has_variable(std::wstring key);
		parsers::where::value_type get_type(std::wstring key);
		bool can_convert(parsers::where::value_type from, parsers::where::value_type to);

	public:
		void error(std::wstring err) { errors.push_back(err); }
		bool has_error() { return !errors.empty(); }
		std::wstring get_error() { return strEx::joinEx(errors, _T(", ")); }
	private:
		std::list<std::wstring> errors;
		object_instance_type object;

	private:
		types_type types;

	};


	struct file_finder_data_arguments : public where_filter::argument_interface {

		typedef where_filter::argument_interface parent_type;
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

		file_finder_data_arguments(std::wstring pattern, int max_depth, parent_type::error_type error, std::wstring syntax, std::wstring datesyntax, bool debug = false);


		bool is_valid_level(int current_level) {
			return (max_level == -1) || (current_level <= max_level);
		}
	};
	typedef where_filter::engine_interface<filter_obj> filter_engine_type;
	typedef file_finder_data_arguments filter_argument_type;
	typedef where_filter::result_counter_interface<filter_obj> filter_result_type;

	struct filesize_engine_interface_type : public filter_engine_type {
		virtual unsigned long long get_size() = 0;
	};


	typedef boost::shared_ptr<filter_engine_type> filter_engine;
	typedef boost::shared_ptr<filesize_engine_interface_type> filesize_engine_interface;
	typedef boost::shared_ptr<filter_argument_type> filter_argument;
	typedef boost::shared_ptr<filter_result_type> filter_result;

	struct factories {
		static filter_engine create_engine(filter_argument arg);
// 		static filesize_engine_interface create_size_engine();
		static filter_result create_result(filter_argument arg);
		static filter_argument create_argument(std::wstring pattern, int max_depth, std::wstring syntax, std::wstring datesyntax);
	};
}
