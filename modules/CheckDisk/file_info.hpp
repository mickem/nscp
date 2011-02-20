#pragma once

#include <file_helpers.hpp>
#include <checkHelpers.hpp>
#include <filter_framework.hpp>


// struct file_finder_data {
// 	file_finder_data(const WIN32_FIND_DATA wfd_, const std::wstring path_) : wfd(wfd_), path(path_) {}
// 	const WIN32_FIND_DATA wfd;
// 	const std::wstring path;
// };
// 
// struct file_info {
// 
// 	std::wstring error;
// 
// 	static file_info get(__int64 now, std::wstring path, std::wstring file) {
// 		return get_2(now, path, file);
// 	}
// 	static file_info get(__int64 now, file_finder_data data) {
// 		return file_info(now, data.wfd, data.path, data.wfd.cFileName);
// 	}
// 	static file_info get(__int64 now, WIN32_FIND_DATA &wfd, std::wstring path) {
// 		return file_info(now, wfd, path, wfd.cFileName);
// 	}
// 
// 	static file_info get_2(__int64 now, std::wstring path, std::wstring file);
// 	static file_info get_1(__int64 now, std::wstring path, std::wstring file);
// 
// // 
// // 	unsigned long long ullSize;
// // 	__int64 ullCreationTime;
// // 	__int64 ullLastAccessTime;
// // 	__int64 ullLastWriteTime;
// // 	__int64 ullNow;
// // 	std::wstring filename;
// // 	std::wstring path;
// // 	std::pair<bool,std::wstring> cached_version;
// // 	std::pair<bool,unsigned long> cached_count;
// // 	DWORD attributes;
// // 
// // 	static const __int64 MSECS_TO_100NS = 10000;
// // 
// // 	__int64 get_creation() {
// // 		return (ullNow-ullCreationTime)/MSECS_TO_100NS;
// // 	}
// // 	__int64 get_access() {
// // 		return (ullNow-ullLastAccessTime)/MSECS_TO_100NS;
// // 	}
// // 	__int64 get_write() {
// // 		return (ullNow-ullLastWriteTime)/MSECS_TO_100NS;
// // 	}
// // 	std::wstring render(std::wstring syntax);
// // 	std::wstring get_version();
// // 	unsigned long get_line_count();
// };
// 
// struct file_container : public file_info {
// 	std::wstring error_;
// 
// 
// 	static file_container get(std::wstring file);
// 
// 	static file_container get(std::wstring file, unsigned long long now);
// 
// 
// 	file_container(__int64 now, const BY_HANDLE_FILE_INFORMATION info, std::wstring file) : file_info(now, info, file_helpers::meta::get_path(file), file_helpers::meta::get_filename(file)) {}
// 	file_container(__int64 now, std::wstring file, std::wstring error) : error_(error), file_info(now, file_helpers::meta::get_path(file), file_helpers::meta::get_filename(file)) {}
// 
// 	bool has_errors() {
// 		return !error_.empty();
// 	}
// 	std::wstring get_error() {
// 		return error_;
// 	}
// 
// };

namespace file_filter {
	struct filter_obj;
}
namespace file_finder {

	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDiscSize> PathContainer;



	struct filter {

		static const int filter_plus = 1;
		static const int filter_minus = 2;
		static const int filter_normal = 3;


		filters::filter_all_numeric<unsigned long long, checkHolders::disk_size_handler<checkHolders::disk_size_type> > size;
		filters::filter_all_times creation;
		filters::filter_all_times accessed;
		filters::filter_all_times written;
		filters::filter_all_strings version;
		filters::filter_all_num_ul line_count;

		inline bool hasFilter() {
			return size.hasFilter() || creation.hasFilter() || 
				accessed.hasFilter() || written.hasFilter();
		}
		bool matchFilter(file_filter::filter_obj *value) const;

		std::wstring getValue() const {
			if (size.hasFilter())
				return _T("size: ") + size.getValue();
			if (creation.hasFilter())
				return _T("creation: ") + creation.getValue();
			if (accessed.hasFilter())
				return _T("accessed: ") + accessed.getValue();
			if (written.hasFilter())
				return _T("written: ") + written.getValue();
			if (version.hasFilter())
				return _T("written: ") + version.getValue();
			if (line_count.hasFilter())
				return _T("written: ") + line_count.getValue();
			return _T("UNknown...");
		}

	};

}