#include "StdAfx.h"
#include "OldCheckDisk.hpp"
#include <time.h>
#include <filter_framework.hpp>
#include <error.hpp>
#include <file_helpers.hpp>
#include <checkHelpers.hpp>
#include <utils.h>
#include <config.h>

#include "file_finder.hpp"
#include "filter.hpp"

NSCAPI::nagiosReturn OldCheckDisk::CheckFile(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf, bool show_errors) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsUInteger> CheckFileContainer;
	if (stl_args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	file_filter::filter_argument args = file_filter::factories::create_argument(_T("*.*"), false, _T("%filename%"));

	//file_finder::file_filter_function finder;
	file_finder::PathContainer tmpObject;
	std::list<std::wstring> paths;
	unsigned int truncate = 0;
	CheckFileContainer query;
	std::wstring alias;
	bool bPerfData = true;
	unsigned int max_dir_depth = -1;
	bool debug = false;

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), args->syntax)
			MAP_OPTIONS_PUSH(_T("path"), paths)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_STR2INT(_T("max-dir-depth"), args->max_level)
			MAP_OPTIONS_PUSH(_T("file"), paths)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), args->debug)
			MAP_OPTIONS_BOOL_EX(_T("filter"), args->bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), args->bFilterAll, _T("all"), _T("any"))
 			MAP_OPTIONS_PUSH_WTYPE(file_finder::filter, _T("filter-size"), size, args->filter_chain)
			MAP_OPTIONS_PUSH_WTYPE(file_finder::filter, _T("filter-creation"), creation, args->filter_chain)
 			MAP_OPTIONS_PUSH_WTYPE(file_finder::filter, _T("filter-written"), written, args->filter_chain)
 			MAP_OPTIONS_PUSH_WTYPE(file_finder::filter, _T("filter-accessed"), accessed, args->filter_chain)
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	}
	file_filter::filter_result result = file_filter::factories::create_result(args);
	file_filter::filter_engine impl = file_filter::factories::create_old_engine(args);
	for (std::list<std::wstring>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {
		file_helpers::patterns::pattern_type path = file_helpers::patterns::split_pattern(*pit);
		file_finder::recursive_scan(result, args, impl, path.first);
		if (args->error->has_error()) {
			if (show_errors)
				message = args->error->get_error();
			else
				message = _T("Check contains error. Check log for details (or enable show_errors in nsc.ini)");
			return NSCAPI::returnUNKNOWN;
		}
	}
	message = result->get_message();
	if (!alias.empty())
		query.alias = alias;
// 	else
// 		query.alias = finder.alias;
	if (query.alias.empty())
		query.alias = _T("no files found");
	unsigned int count = result->get_match_count();
	query.runCheck(count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("CheckFile ok");
	return returnCode;
}

#define MAP_FILTER(value, obj) \
		else if (p__.first == _T("filter+"##value)) { file_finder::filter filter; filter.obj = p__.second; \
			args->old_chain.push_back(filteritem_type(file_finder::filter::filter_plus, filter)); } \
		else if (p__.first == _T("filter-"##value)) { file_finder::filter filter; filter.obj = p__.second; \
			args->old_chain.push_back(filteritem_type(file_finder::filter::filter_minus, filter)); } \
		else if (p__.first == _T("filter."##value)) { file_finder::filter filter; filter.obj = p__.second; \
			args->old_chain.push_back(filteritem_type(file_finder::filter::filter_normal, filter)); }

NSCAPI::nagiosReturn OldCheckDisk::CheckFile2(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf, bool show_errors) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsUInteger> CheckFileContainer;
	typedef std::pair<int,file_finder::filter> filteritem_type;
	typedef std::list<filteritem_type> filterlist_type;
	if (stl_args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	//file_finder::file_filter_function_ex finder;
	file_finder::PathContainer tmpObject;
	std::list<std::wstring> paths;
	unsigned int truncate = 0;
	CheckFileContainer query;
	std::wstring masterSyntax = _T("%list%");
	std::wstring alias;
	bool bPerfData = true;
	bool ignoreError = false;

	file_filter::filter_argument args = file_filter::factories::create_argument(_T("*.*"), false, _T("%filename%"));

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), args->syntax)
			MAP_OPTIONS_STR(_T("master-syntax"), masterSyntax)
			MAP_OPTIONS_PUSH(_T("path"), paths)
			MAP_OPTIONS_STR(_T("pattern"), args->pattern)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_PUSH(_T("file"), paths)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), args->debug)
			MAP_OPTIONS_BOOL_TRUE(_T("ignore-errors"), ignoreError)
			MAP_OPTIONS_STR2INT(_T("max-dir-depth"), args->max_level)
			MAP_OPTIONS_BOOL_EX(_T("filter"), args->bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), args->bFilterAll, _T("all"), _T("any"))
			/*
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-size"), fileSize, finder.filter_chain)
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-creation"), fileCreation, finder.filter_chain)
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-written"), fileWritten, finder.filter_chain)
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-accessed"), fileAccessed, finder.filter_chain)
			*/

			MAP_FILTER(_T("size"), size)
			MAP_FILTER(_T("creation"), creation)
			MAP_FILTER(_T("written"), written)
			MAP_FILTER(_T("accessed"), accessed)
			MAP_FILTER(_T("version"), version)
			MAP_FILTER(_T("line-count"), line_count)
/*
			MAP_FILTER(_T("filter.size"), size, filter_normal)
			MAP_FILTER(_T("filter.creation"), creation, filter_normal)
			MAP_FILTER(_T("filter.written"), written, filter_normal)
			MAP_FILTER(_T("filter.accessed"), accessed, filter_normal)
			MAP_FILTER(_T("filter.version"), version, filter_normal)

			MAP_FILTER(_T("filter-size"), size, filter_minus)
			MAP_FILTER(_T("filter-creation"), creation, filter_minus)
			MAP_FILTER(_T("filter-written"), written, filter_minus)
			MAP_FILTER(_T("filter-accessed"), accessed, filter_minus)
			MAP_FILTER(_T("filter-version"), version, filter_minus)
*/
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	}
	if (paths.empty()) {
		message = _T("Missing path argument");
		return NSCAPI::returnUNKNOWN;
	}
	file_filter::filter_engine impl = file_filter::factories::create_old_engine(args);
	if (!impl) {
		message = _T("Missing filter argument");
		return NSCAPI::returnUNKNOWN;
	}
	if (!impl->validate(message))
		return NSCAPI::returnUNKNOWN;
	if (args->debug)
		NSC_DEBUG_MSG_STD(_T("NOW: ") + strEx::format_filetime(args->now));

	file_filter::filter_result result = file_filter::factories::create_result(args);
	for (std::list<std::wstring>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {

		file_finder::recursive_scan(result, args, impl, *pit);

		//file_finder::recursive_scan<file_finder::file_filter_function_ex>(*pit, pattern, 0, max_dir_depth, finder, &errors, debug);
		if (!ignoreError && args->error->has_error()) {
			if (show_errors)
				message = args->error->get_error();
			else
				message = _T("Check contains error. Check log for details (or enable show_errors in nsc.ini)");
			return NSCAPI::returnUNKNOWN;
		}
	}
	if (!alias.empty())
		query.alias = alias;
	else
		query.alias = _T("found files");
	unsigned int count = result->get_match_count();
	query.runCheck(count, returnCode, message, perf);
	message = result->render(masterSyntax, returnCode);
	if ((truncate > 0) && (message.length() > (truncate-4))) {
		message = message.substr(0, truncate-4) + _T("...");
		//perf = _T("");
	}
	if (message.empty())
		message = _T("CheckFile ok");
	return returnCode;
}

