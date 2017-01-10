/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/helpers.hpp>

#include <error/error.hpp>

#include <format.hpp>
#include <str/utils.hpp>

#ifdef WIN32
#include <Windows.h>
#endif

#include <map>
#include <string>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>



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
			: is_total_(false)
			, ullCreationTime(0)
			, ullLastAccessTime(0)
			, ullLastWriteTime(0)
			, ullSize(0)
			, ullNow(0) {}
		filter_obj(boost::filesystem::path path_, std::string filename_, __int64 now = 0, __int64 creationTime = 0, __int64 lastAccessTime = 0, __int64 lastWriteTime = 0, __int64 size = 0, DWORD attributes = 0)
			: is_total_(false)
			, path(path_)
			, filename(filename_)
			, ullCreationTime(creationTime)
			, ullLastAccessTime(lastAccessTime)
			, ullLastWriteTime(lastWriteTime)
			, ullSize(size)
			, ullNow(now)
			, attributes(attributes) {}

		filter_obj(const filter_obj& other)
			: ullSize(other.ullSize)
			, ullCreationTime(other.ullCreationTime)
			, ullLastAccessTime(other.ullLastAccessTime)
			, ullLastWriteTime(other.ullLastWriteTime)
			, ullNow(other.ullNow)
			, filename(other.filename)
			, path(other.path)
			, cached_version(other.cached_version)
			, cached_count(other.cached_count)
			, attributes(other.attributes) {}

		const filter_obj& operator=(const filter_obj&other) {
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

#ifdef WIN32
		static boost::shared_ptr<filter_obj> get(unsigned long long now, const WIN32_FIND_DATA info, boost::filesystem::path path);
#endif
		static boost::shared_ptr<file_filter::filter_obj> get_total(unsigned long long now);
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
		long long get_age() {
			long long now = parsers::where::constants::get_now();
			return now - get_write();
		}
		__int64 to_local_time(const __int64  &t) {
			FILETIME ft;
			ft.dwHighDateTime = t >> 32;
			ft.dwLowDateTime = t;
			FILETIME lft = ft_utc_to_local_time(ft);
			return (lft.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)lft.dwLowDateTime;
		}

		FILETIME ft_utc_to_local_time(const FILETIME &ft) {
			FILETIME lft;
			SYSTEMTIME st1, st2;
			FileTimeToSystemTime(&ft, &st1);
			SystemTimeToTzSpecificLocalTime(NULL, &st1, &st2);
			SystemTimeToFileTime(&st2, &lft);
			return lft;
		}

		std::string get_creation_su() {
			return format::format_filetime(ullCreationTime);
		}
		std::string get_access_su() {
			return format::format_filetime(ullLastAccessTime);
		}
		std::string get_written_su() {
			return format::format_filetime(ullLastWriteTime);
		}
		std::string get_creation_sl() {
			return format::format_filetime(to_local_time(ullCreationTime));
		}
		std::string get_access_sl() {
			return format::format_filetime(to_local_time(ullLastAccessTime));
		}
		std::string get_written_sl() {
			return format::format_filetime(to_local_time(ullLastWriteTime));
		}
		unsigned long long get_type();
		std::string get_type_su();

		unsigned long long get_size() { return ullSize; }
		//		std::string render(std::string syntax, std::string datesyntax);
		std::string get_version();
		unsigned long get_line_count();

	public:
		void add(boost::shared_ptr<file_filter::filter_obj> info);
		void make_total() { is_total_ = true; }
		bool is_total() const { return is_total_; }

		unsigned long long ullSize;
		__int64 ullCreationTime;
		__int64 ullLastAccessTime;
		__int64 ullLastWriteTime;
		__int64 ullNow;
		std::string filename;
		bool is_total_;
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