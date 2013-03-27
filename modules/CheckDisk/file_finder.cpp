#include "StdAfx.h"

#include "file_finder.hpp"

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

void file_finder::recursive_scan(file_filter::filter_result result, file_filter::filter_argument args, file_filter::filter_engine engine, boost::filesystem::path dir, bool recursive, int current_level) {
	if (!args->is_valid_level(current_level)) {
		if (args->debug) args->error->report_debug("Level deapth exausted: " + strEx::s::xtos(current_level));
		return;
	}
	WIN32_FIND_DATA wfd;

	DWORD fileAttr = GetFileAttributes(dir.wstring().c_str());
	if ((fileAttr == INVALID_FILE_ATTRIBUTES)&&(!recursive)) {
		args->error->report_error("Invalid file specified: " + dir.string());
	} else if (fileAttr == INVALID_FILE_ATTRIBUTES) {
		args->error->report_warning("Invalid file specified: " + dir.string());
	}
	if (args->debug) args->error->report_debug("Input is: " + dir.string() + " / " + strEx::s::xtos(fileAttr));

	if (!file_helpers::checks::is_directory(fileAttr)) {
		if (args->debug) args->error->report_debug("Found a file won't do recursive scan: " + dir.string());
		// It is a file check it an return (don't check recursively)
		file_helpers::patterns::pattern_type single_path = file_helpers::patterns::split_path_ex(dir.string());
		if (args->debug) args->error->report_debug("Path is: " + single_path.first.string());
		HANDLE hFind = FindFirstFile(dir.wstring().c_str(), &wfd);
		if (hFind != INVALID_HANDLE_VALUE) {
			boost::shared_ptr<file_filter::filter_obj> info = file_filter::filter_obj::get(args->now, wfd, single_path.first);
			if (engine) 
				result->process(info, engine->match(info));
			else
				result->process(info, true);
			FindClose(hFind);
		} else {
			args->error->report_error("File was NOT found!");
		}
		return;
	}
	std::string file_pattern = dir.string() + "\\" + args->pattern;
	if (args->debug) args->error->report_debug("File pattern: " + file_pattern);
	HANDLE hFind = FindFirstFile(utf8::cvt<std::wstring>(file_pattern).c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (
				file_helpers::checks::is_directory(wfd.dwFileAttributes)
				&& ( wcscmp(wfd.cFileName, _T(".")) != 0 || wcscmp(wfd.cFileName, _T("..")) != 0)
				)
				continue;
			boost::shared_ptr<file_filter::filter_obj> info = file_filter::filter_obj::get(args->now, wfd, dir);
			if (engine) 
				result->process(info, engine->match(info));
			else
				result->process(info, true);
		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	std::string dir_pattern = dir.string() + "\\*.*";
	if (args->debug) args->error->report_debug("File pattern: " + dir_pattern);
	hFind = FindFirstFile(utf8::cvt<std::wstring>(dir_pattern).c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (file_helpers::checks::is_directory(wfd.dwFileAttributes)) {
				if ( (wcscmp(wfd.cFileName, _T(".")) != 0) && (wcscmp(wfd.cFileName, _T("..")) != 0) )
					recursive_scan(result, args, engine, dir / wfd.cFileName, true, current_level+1);
			}
		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
}
