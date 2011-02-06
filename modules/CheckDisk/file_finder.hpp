#pragma once

#include <boost/shared_ptr.hpp>

#include <file_helpers.hpp>
#include <strEx.h>
#include <checkHelpers.hpp>
#include <filter_framework.hpp>

#include "file_info.hpp"

#include "filter.hpp"

namespace file_finder {

	//typedef where_filter::argument<file_finder_data_arguments> file_finder_arguments;
	//typedef file_finder_data_arguments file_finder_arguments;
	//typedef where_filter::simple_count_result<file_info> file_finder_result;

// 	struct factory {
// 		static file_finder_arguments create_arg(std::wstring syntax, bool debug, std::wstring pattern, int max_depth = -1) {
// 			return file_finder_arguments (pattern, max_depth, syntax, debug);
// 		}
// 	};

//	typedef where_filter::engine_interface<file_info> file_engine_interface;

// 	struct file_size_engine : public file_engine_interface {
// 		file_size_engine() : size(0) {}
// 
// 		bool boot() { return true; }
// 		bool validate(std::wstring &message) {
// 			return true;
// 		}
// 		bool match(file_info &record) {
// 			if (!file_helpers::checks::is_directory(record.attributes)) {
// 				size += record.ullSize;
// 			}
// 			return true;
// 		}
// 		std::wstring get_name() { return _T("file-size"); }
// 		std::wstring get_subject() { return _T("TODO"); }
// 		inline unsigned long long getSize() {
// 			return size;
// 		}
// 	private:  
// 		unsigned long long size;
// 
// 	};


	void recursive_scan(file_filter::filter_result result, file_filter::filter_argument args, file_filter::filter_engine engine, std::wstring dir, bool recursive = false, int current_level = 0);

		
// 	struct find_first_file_info : public baseFinderFunction
// 	{
// 		file_info info;
// 		__int64 now_;
// 	//	std::wstring message;
// 		find_first_file_info() : now_(0) {
// 			FILETIME now;
// 			GetSystemTimeAsFileTime(&now);
// 			now_ = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
// 		}
// 		result_type operator()(argument_type ffd) {
// 			if (file_helpers::checks::is_directory(ffd.wfd.dwFileAttributes))
// 				return true;
// 
// 			file_info info = file_info::get(now_, ffd);
// // 			if (!info.error.empty()) {
// // 				setError(ffd.errors, info.error);
// // 				return false;
// // 			}
// 			return false;
// 			/*
// 			BY_HANDLE_FILE_INFORMATION _info;
// 
// 			HANDLE hFile = CreateFile((ffd.path + _T("\\") + ffd.wfd.cFileName).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
// 				0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
// 			if (hFile == INVALID_HANDLE_VALUE) {
// 				setError(ffd.errors, _T("Could not open file: ") + ffd.path + _T("\\") + ffd.wfd.cFileName + _T(": ") + error::lookup::last_error());
// 				return false;
// 			}
// 			GetFileInformationByHandle(hFile, &_info);
// 			CloseHandle(hFile);
// 			info = file_info(_info, ffd.path, ffd.wfd.cFileName);
// 			return false;
// 			*/
// 		}
// 		inline void setError(error_reporter *errors, std::wstring msg) {
// 			if (errors != NULL)
// 				errors->report_error(msg);
// 		}
// 	};


// 	struct file_filter_function : public baseFinderFunction
// 	{
// 		std::list<file_filter> filter_chain;
// 		bool bFilterAll;
// 		bool bFilterIn;
// 		std::wstring message;
// 		std::wstring syntax;
// 		std::wstring alias;
// 		unsigned long long now;
// 		unsigned int hit_count;
// 		__int64 now_;
// 
// 		file_filter_function() : now_(0), hit_count(0), bFilterIn(true), bFilterAll(true) {
// 			FILETIME now;
// 			GetSystemTimeAsFileTime(&now);
// 			now_ = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
// 		}
// 		result_type operator()(argument_type ffd) {
// 			if (file_helpers::checks::is_directory(ffd.wfd.dwFileAttributes))
// 				return true;
// 
// 			file_info info = file_info::get(now, ffd);
// // 			if (!info.error.empty()) {
// // 				setError(ffd.errors, info.error);
// // 				return true;
// // 			}
// 			/*
// 			BY_HANDLE_FILE_INFORMATION _info;
// 
// 			HANDLE hFile = CreateFile((ffd.path + _T("\\") + ffd.wfd.cFileName).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
// 				0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
// 			if (hFile == INVALID_HANDLE_VALUE) {
// 				setError(ffd.errors, _T("Could not open file: ") + ffd.path + _T("\\") + ffd.wfd.cFileName + _T(": ") + error::lookup::last_error());
// 				return true;
// 			}
// 			GetFileInformationByHandle(hFile, &_info);
// 			CloseHandle(hFile);
// 			file_info info(_info, ffd.path, ffd.wfd.cFileName);
// 			info.ullNow = now;
// 			*/
// 
// 			for (std::list<file_finder::file_filter>::const_iterator cit3 = filter_chain.begin(); cit3 != filter_chain.end(); ++cit3 ) {
// 				bool bMatch = bFilterAll;
// 				bool bTmpMatched = (*cit3).matchFilter(info);
// 				if (bFilterAll) {
// 					if (!bTmpMatched) {
// 						bMatch = false;
// 						break;
// 					}
// 				} else {
// 					if (bTmpMatched) {
// 						bMatch = true;
// 						break;
// 					}
// 				}
// 				if ((bFilterIn&&bMatch)||(!bFilterIn&&!bMatch)) {
// 					strEx::append_list(message, info.render(syntax));
// 					if (alias.length() < 16)
// 						strEx::append_list(alias, info.filename);
// 					else
// 						strEx::append_list(alias, std::wstring(_T("...")));
// 					hit_count++;
// 				}
// 			}
// 			return true;
// 		}
// 		inline void setError(error_reporter *errors, std::wstring msg) {
// 			if (errors != NULL)
// 				errors->report_error(msg);
// 		}
// 	};
// 
// 
// 
// 	struct file_filter_function_ex : public baseFinderFunction
// 	{
// 		static const int filter_plus = 1;
// 		static const int filter_minus = 2;
// 		static const int filter_normal = 3;
// 
// 		typedef std::pair<int,file_filter> filteritem_type;
// 		typedef std::list<filteritem_type > filterlist_type;
// 		filterlist_type filter_chain;
// 		bool bFilterAll;
// 		bool bFilterIn;
// 		bool debug_;
// 		std::wstring message;
// 		std::wstring syntax;
// 		//std::wstring alias;
// 		unsigned long long now_;
// 		unsigned int hit_count;
// 		unsigned int file_count;
// 		boost::shared_ptr<filter_engine> filter_impl;
// 
// 		file_filter_function_ex() : hit_count(0), file_count(0), debug_(false), bFilterIn(true), bFilterAll(true), now_(0) {
// 			FILETIME now;
// 			GetSystemTimeAsFileTime(&now);
// 			now_ = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
// 
// 
// 		}
// 		bool boot(std::wstring message) {
// 			if (!filter_impl) {
// 				message = _T("Failed to initialize filter subsystem.");
// 				return false;
// 			}
// 			filter_impl->boot();
// 		}
// 		result_type operator()(argument_type ffd) {
// 			if (file_helpers::checks::is_directory(ffd.wfd.dwFileAttributes))
// 				return true;
// 
// 			file_info info = file_info::get(now_, ffd);
// // 			if (!info.error.empty()) {
// // 				setError(ffd.errors, info.error);
// // 				return true;
// // 			}
// // 			bool match = filter_impl->match(&info);
// // 
// // 			if (match) {
// // 				if (!data.syntax.empty()) {
// // 					strEx::append_list(message, record.render(data.bShowDescriptions, data.syntax));
// // 				} else if (!data.bShowDescriptions) {
// // 					strEx::append_list(message, record.eventSource());
// // 				} else {
// // 					strEx::append_list(message, record.eventSource());
// // 					message += _T("(") + EventLogRecord::translateType(record.eventType()) + _T(", ") + 
// // 						strEx::itos(record.eventID()) + _T(", ") + EventLogRecord::translateSeverity(record.severity()) + _T(")");
// // 					message += _T("[") + record.enumStrings() + _T("]");
// // 				}
// // 				hit_count++;
// // 			}
// 
// // 			bool bMatch = !bFilterIn;
// // 			for (filterlist_type::const_iterator cit3 = filter_chain.begin(); cit3 != filter_chain.end(); ++cit3 ) {
// // 				bool bTmpMatched = (*cit3).second.matchFilter(info);
// // 				int mode = (*cit3).first;
// // 
// // 				if ((mode == filter_minus)&&(bTmpMatched)) {
// // 					// a -<filter> hit so thrash item and bail out!
// // 					if (debug_)
// // 						NSC_DEBUG_MSG_STD(_T("Matched: - ") + (*cit3).second.getValue() + _T(" for: ") + info.render(syntax));
// // 					bMatch = false;
// // 					break;
// // 				} else if ((mode == filter_plus)&&(!bTmpMatched)) {
// // 					// a +<filter> missed hit so thrash item and bail out!
// // 					if (debug_)
// // 						NSC_DEBUG_MSG_STD(_T("Matched (missed): + ") + (*cit3).second.getValue() + _T(" for: ") + info.render(syntax));
// // 					bMatch = false;
// // 					break;
// // 				} else if (bTmpMatched) {
// // 					if (debug_)
// // 						NSC_DEBUG_MSG_STD(_T("Matched: . (contiunue): ") + (*cit3).second.getValue() + _T(" for: ") + info.render(syntax));
// // 					bMatch = true;
// // 				}
// // 			}
// 
// 			//NSC_DEBUG_MSG_STD(_T("result: ") + strEx::itos(bFilterIn) + _T(" -- ") + strEx::itos(bMatch));
// // 			if ((bFilterIn&&bMatch)||(!bFilterIn&&!bMatch)) {
// // 				strEx::append_list(message, info.render(syntax));
// // 				/*
// // 				if (alias.length() < 16)
// // 					strEx::append_list(alias, info.filename);
// // 				else
// // 					strEx::append_list(alias, std::wstring(_T("...")));
// // 					*/
// // 				hit_count++;
// // 			}
// 			file_count++;
// 			return true;
// 		}
// 		inline void setError(error_reporter *errors, std::wstring msg) {
// 			if (errors != NULL)
// 				errors->report_error(msg);
// 		}
// 
// 		std::wstring render(std::wstring syntax) {
// 			strEx::replace(syntax, _T("%list%"), message);
// 			strEx::replace(syntax, _T("%matches%"), strEx::itos(hit_count));
// 			strEx::replace(syntax, _T("%files%"), strEx::itos(file_count));
// 			return syntax;
// 		}
// 
// 		bool has_filter() {
// 			return !filter_chain.empty();
// 		}
// 
// 	};


}