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

#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include "file_info.hpp"


namespace file_filter {
	struct file_object_exception : public std::exception {
		std::string error_;
	public:
		file_object_exception(std::string error) : error_(error) {}
		~file_object_exception() throw() {}
		const char* what() const throw() {
			return error_.c_str();
		}
	};


	struct filter_obj {
		filter_obj() 
			: ullCreationTime(0)
			, ullLastAccessTime(0)
			, ullLastWriteTime(0)
			, ullSize(0)
			, ullNow(0)
		{}
		filter_obj(boost::filesystem::path path_, std::string filename_, __int64 now = 0, __int64 creationTime = 0, __int64 lastAccessTime = 0, __int64 lastWriteTime = 0, __int64 size = 0, DWORD attributes = 0) 
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
		static filter_obj get(unsigned long long now, const WIN32_FILE_ATTRIBUTE_DATA info, boost::filesystem:: path path, std::string filename);
		static filter_obj get(unsigned long long now, const BY_HANDLE_FILE_INFORMATION info, boost::filesystem::path path, std::string filename);
		static boost::shared_ptr<filter_obj>  get(unsigned long long now, const WIN32_FIND_DATA info, boost::filesystem::path path);
#endif
		static filter_obj get(unsigned long long now, boost::filesystem::path path, std::string filename);
		static filter_obj get(unsigned long long now, std::string file);
		static filter_obj get(std::string file);

		std::string get_filename() { return filename; }
		std::string get_path(parsers::where::evaluation_context) { return path.string(); }

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
		std::string render(std::string syntax, std::string datesyntax);
		std::string get_version();
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
		std::string filename;
		boost::filesystem::path path;
		boost::optional<std::string> cached_version;
		boost::optional<unsigned long> cached_count;
		DWORD attributes;
	};


	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}
