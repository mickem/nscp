#include "StdAfx.h"

#include "file_finder.hpp"

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

void file_finder::recursive_scan(file_filter::filter_result result, file_filter::filter_argument args, file_filter::filter_engine engine, boost::filesystem::wpath dir, bool recursive, int current_level) {
	if (!args->is_valid_level(current_level)) {
		if (args->debug) args->error->report_debug(_T("Level deapth exausted: ") + strEx::itos(current_level));
		return;
	}
	WIN32_FIND_DATA wfd;

	DWORD fileAttr = GetFileAttributes(dir.string().c_str());
	if ((fileAttr == INVALID_FILE_ATTRIBUTES)&&(!recursive)) {
		args->error->report_error(_T("Invalid file specified: ") + dir.string());
	} else if (fileAttr == INVALID_FILE_ATTRIBUTES) {
		args->error->report_warning(_T("Invalid file specified: ") + dir.string());
	}
	if (args->debug) args->error->report_debug(_T("Input is: ") + dir.string() + _T(" / ") + strEx::ihextos(fileAttr));

	if (!file_helpers::checks::is_directory(fileAttr)) {
		if (args->debug) args->error->report_debug(_T("Found a file dont do recursive scan: ") + dir.string());
		// It is a file check it an return (don't check recursively)
		file_helpers::patterns::pattern_type single_path = file_helpers::patterns::split_path_ex(dir.string());
		if (args->debug) args->error->report_debug(_T("Path is: ") + single_path.first.string());
		HANDLE hFind = FindFirstFile(dir.string().c_str(), &wfd);
		if (hFind != INVALID_HANDLE_VALUE) {
			boost::shared_ptr<file_filter::filter_obj> info = file_filter::filter_obj::get(args->now, wfd, single_path.first);
			result->process(info, engine->match(info));
			FindClose(hFind);
		} else {
			args->error->report_error(_T("File was NOT found!"));
		}
		return;
	}
	std::wstring file_pattern = dir.string() + _T("\\") + args->pattern;
	if (args->debug) args->error->report_debug(_T("File pattern: ") + file_pattern);
	HANDLE hFind = FindFirstFile(file_pattern.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			boost::shared_ptr<file_filter::filter_obj> info = file_filter::filter_obj::get(args->now, wfd, dir);
			result->process(info, engine->match(info));
		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	std::wstring dir_pattern = dir.string() + _T("\\*.*");
	if (args->debug) args->error->report_debug(_T("File pattern: ") + dir_pattern);
	hFind = FindFirstFile(dir_pattern.c_str(), &wfd);
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
