/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"
#include "CheckDisk.h"
#include <strEx.h>
#include <time.h>
#include <filter_framework.hpp>
#include <error.hpp>
#include <file_helpers.hpp>
#include <checkHelpers.hpp>

#include "file_info.hpp"
#include "file_finder.hpp"
#include "OldCheckDisk.hpp"

#include <simple_timer.hpp>

#include "filter.hpp"

CheckDisk gCheckDisk;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

CheckDisk::CheckDisk() : show_errors_(false) {
}
CheckDisk::~CheckDisk() {
}


bool CheckDisk::loadModule() {
	try {
		NSCModuleHelper::registerCommand(_T("CheckFileSize"), _T("Check or directory a file and verify its size."));
		NSCModuleHelper::registerCommand(_T("CheckDriveSize"), _T("Check the size (free-space) of a drive or volume."));
		NSCModuleHelper::registerCommand(_T("CheckFile"), _T("Check various aspects of a file and/or folder."));

		show_errors_ = NSCModuleHelper::getSettingsInt(CHECK_DISK_SECTION_TITLE, CHECK_DISK_SHOW_ERRORS, CHECK_DISK_SHOW_ERRORS_DEFAULT)==1;
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	return true;
}
bool CheckDisk::unloadModule() {
	return true;
}

bool CheckDisk::hasCommandHandler() {
	return true;
}
bool CheckDisk::hasMessageHandler() {
	return false;
}







NSCAPI::nagiosReturn CheckDisk::CheckDriveSize(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnCRIT;
	}

	DriveContainer tmpObject;
	bool bFilter = false;
	bool bFilterRemote = false;
	bool bFilterRemovable = false;
	bool bFilterFixed = false;
	bool bFilterCDROM = false;
	bool bFilterNoRootDir = false;
	bool bCheckAllDrives = false;
	bool bCheckAllOthers = false;
	bool bNSClient = false;
	bool bPerfData = true;
	std::list<DriveContainer> drives;
	std::wstring strCheckAll;

	MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR_AND(_T("Drive"), tmpObject.data, drives.push_back(tmpObject))
		MAP_OPTIONS_DISK_ALL(tmpObject, _T(""), _T("Free"), _T("Used"))
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterFixed, _T("FIXED"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterCDROM, _T("CDROM"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterRemovable, _T("REMOVABLE"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterRemote, _T("REMOTE"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterNoRootDir, _T("NO_ROOT_DIR"))
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		//MAP_OPTIONS_BOOL_TRUE(CHECK_ALL, bCheckAll)
		MAP_OPTIONS_STR(CHECK_ALL, strCheckAll)
		MAP_OPTIONS_BOOL_TRUE(CHECK_ALL_OTHERS, bCheckAllOthers)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
			else if (p2.first == _T("Drive")) {
				tmpObject.data = p__.second;
				tmpObject.alias = p2.second;
				drives.push_back(tmpObject);
			}
		MAP_OPTIONS_MISSING_EX(p2, message, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
	MAP_OPTIONS_FALLBACK_AND(tmpObject.data, drives.push_back(tmpObject))
	MAP_OPTIONS_END()
	bFilter = bFilterFixed || bFilterCDROM  || bFilterRemote || bFilterRemovable;

	if ((drives.size() == 0) && strCheckAll.empty())
		bCheckAllDrives = true;

	if (strCheckAll == _T("volumes")) {

		DWORD bufSize = GetLogicalDriveStrings(0, NULL)+5;
		TCHAR *buffer = new TCHAR[bufSize+10];
		if (GetLogicalDriveStrings(bufSize, buffer)>0) {
			while (buffer[0] != 0) {
				std::wstring drv = buffer;

				UINT drvType = GetDriveType(drv.c_str());
				if ( 
					((!bFilter)&&(drvType == DRIVE_FIXED))  ||
					((bFilter)&&(bFilterFixed)&&(drvType==DRIVE_FIXED)) ||
					((bFilter)&&(bFilterCDROM)&&(drvType==DRIVE_CDROM)) ||
					((bFilter)&&(bFilterRemote)&&(drvType==DRIVE_REMOTE)) ||
					((bFilter)&&(bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) ||
					((bFilter)&&(bFilterNoRootDir)&&(drvType==DRIVE_NO_ROOT_DIR)) 
					)
					drives.push_back(DriveContainer(drv, tmpObject.warn, tmpObject.crit));
				else
					NSC_DEBUG_MSG_STD(_T("Ignoring drive: ") + drv);

				buffer = &buffer[drv.size()];
				buffer++;
			}
		} else {
			NSC_LOG_ERROR_STD(_T("Failed to get buffer size: ") + error::lookup::last_error());
		}
	}

	if (bCheckAllDrives) {
		DWORD dwDrives = GetLogicalDrives();
		int idx = 0;
		while (dwDrives != 0) {
			if (dwDrives & 0x1) {
				std::wstring drv;
				drv += static_cast<TCHAR>('A' + idx); drv += _T(":\\");
				UINT drvType = GetDriveType(drv.c_str());
				if ( ((!bFilter)&&(drvType == DRIVE_FIXED))  ||
					((bFilter)&&(bFilterFixed)&&(drvType==DRIVE_FIXED)) ||
					((bFilter)&&(bFilterCDROM)&&(drvType==DRIVE_CDROM)) ||
					((bFilter)&&(bFilterRemote)&&(drvType==DRIVE_REMOTE)) ||
					((bFilter)&&(bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) ||
					((bFilter)&&(bFilterNoRootDir)&&(drvType==DRIVE_NO_ROOT_DIR)) 
					)
					drives.push_back(DriveContainer(drv, tmpObject.warn, tmpObject.crit));
			}
			idx++;
			dwDrives >>= 1;
		}
	}
	if (bCheckAllOthers) {
		std::list<DriveContainer> checkdrives;
		DWORD dwDrives = GetLogicalDrives();
		int idx = 0;
		while (dwDrives != 0) {
			if (dwDrives & 0x1) {
				std::wstring drv;
				drv += static_cast<TCHAR>('A' + idx); drv += _T(":\\");
				UINT drvType = GetDriveType(drv.c_str());
				if ( ((!bFilter)&&(drvType == DRIVE_FIXED))  ||
					((bFilter)&&(bFilterFixed)&&(drvType==DRIVE_FIXED)) ||
					((bFilter)&&(bFilterCDROM)&&(drvType==DRIVE_CDROM)) ||
					((bFilter)&&(bFilterRemote)&&(drvType==DRIVE_REMOTE)) ||
					((bFilter)&&(bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) ||
					((bFilter)&&(bFilterNoRootDir)&&(drvType==DRIVE_NO_ROOT_DIR)) 
					)  
				{
					bool bFound = false;
					for (std::list<DriveContainer>::const_iterator pit = drives.begin();pit!=drives.end();++pit) {
						DriveContainer drive = (*pit);
						if (_wcsicmp(drive.data.substr(0,1).c_str(), drv.substr(0,1).c_str())==0)
							bFound = true;
					}
					if (!bFound)
						checkdrives.push_back(DriveContainer(drv, tmpObject.warn, tmpObject.crit));
				}
			}
			idx++;
			dwDrives >>= 1;
		}
		drives = checkdrives;
	}


	for (std::list<DriveContainer>::const_iterator pit = drives.begin();pit!=drives.end();++pit) {
		DriveContainer drive = (*pit);
		if (drive.data.length() == 1)
			drive.data += _T(":");
		drive.perfData = bPerfData;
		UINT drvType = GetDriveType(drive.data.c_str());

		if ((!bFilter)&&!((drvType == DRIVE_FIXED)||(drvType == DRIVE_NO_ROOT_DIR))) {
			message = _T("UNKNOWN: Drive is not a fixed drive: ") + drive.getAlias() + _T(" (it is a ") + get_filter(drvType) + _T(" drive)");
			return NSCAPI::returnUNKNOWN;
		} else if ( (bFilter)&&
			(
			((!bFilterFixed)&&((drvType==DRIVE_FIXED)||(drvType==DRIVE_NO_ROOT_DIR))) ||
			((!bFilterCDROM)&&(drvType==DRIVE_CDROM)) ||
			((!bFilterRemote)&&(drvType==DRIVE_REMOTE)) ||
			((!bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) 
			)) {
				message = _T("UNKNOWN: Drive does not match the current filter: ") + drive.getAlias() + _T(" (add FilterType=") + get_filter(drvType) + _T(" to check this drive)");
				return NSCAPI::returnUNKNOWN;
		}

		ULARGE_INTEGER freeBytesAvailableToCaller;
		ULARGE_INTEGER totalNumberOfBytes;
		ULARGE_INTEGER totalNumberOfFreeBytes;
		if (!GetDiskFreeSpaceEx(drive.data.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
			message = _T("CRITICAL: Could not get free space for: ") + drive.getAlias() + _T(" ") + drive.data + _T(" reason: ") + error::lookup::last_error();
			return NSCAPI::returnCRIT;
		}

		if (bNSClient) {
			if (!message.empty())
				message += _T("&");
			message += strEx::itos(totalNumberOfFreeBytes.QuadPart);
			message += _T("&");
			message += strEx::itos(totalNumberOfBytes.QuadPart);
		} else {
			checkHolders::PercentageValueType<checkHolders::disk_size_type, checkHolders::disk_size_type> value;
			std::wstring tstr;
			value.value = totalNumberOfBytes.QuadPart-totalNumberOfFreeBytes.QuadPart;
			value.total = totalNumberOfBytes.QuadPart;
			drive.setDefault(tmpObject);
			drive.runCheck(value, returnCode, message, perf);
		}
	}
	if (message.empty())
		message = _T("OK: All drives within bounds.");
	else if (!bNSClient)
		message = NSCHelper::translateReturn(returnCode) + _T(": ") + message;
	return returnCode;
}

std::wstring CheckDisk::get_filter(unsigned int drvType) {
	if (drvType==DRIVE_FIXED)
		return _T("FIXED");
	if (drvType==DRIVE_NO_ROOT_DIR)
		return _T("NO_ROOT_DIR");
	if (drvType==DRIVE_CDROM)
		return _T("CDROM");
	if (drvType==DRIVE_REMOTE)
		return _T("REMOTE");
	if (drvType==DRIVE_REMOVABLE)
		return _T("REMOVABLE");
	return _T("unknown: ") + strEx::itos(drvType);
}





NSCAPI::nagiosReturn CheckDisk::CheckFileSize(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	bool bPerfData = true;
	bool debug = false;
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	file_finder::PathContainer tmpObject;
	std::list<file_finder::PathContainer> paths;

	MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR_AND(_T("File"), tmpObject.data, paths.push_back(tmpObject))
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_STR(_T("MaxWarn"), tmpObject.warn.max)
		MAP_OPTIONS_STR(_T("MinWarn"), tmpObject.warn.min)
		MAP_OPTIONS_STR(_T("MaxCrit"), tmpObject.crit.max)
		MAP_OPTIONS_STR(_T("MinCrit"), tmpObject.crit.min)
		MAP_OPTIONS_BOOL_TRUE(_T("debug"), debug)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_SECONDARY_BEGIN(_T(":"), p2)
		else if (p2.first == _T("File")) {
			tmpObject.data = p__.second;
			tmpObject.alias = p2.second;
			paths.push_back(tmpObject);
		}
		MAP_OPTIONS_MISSING_EX(p2, message, _T("Unknown argument: "))
		MAP_OPTIONS_SECONDARY_END()
		MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
	MAP_OPTIONS_END()

	file_filter::filesize_engine_interface impl = file_filter::factories::create_size_engine();

	for (std::list<file_finder::PathContainer>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {
		file_finder::PathContainer path = (*pit);
		std::wstring tstr;
		std::wstring sName = path.getAlias();
		//file_finder::get_size sizeFinder;
		file_helpers::patterns::pattern_type splitpath = file_helpers::patterns::split_pattern(path.data);

		file_filter::filter_argument argument = file_filter::factories::create_argument(splitpath.second, debug, _T(""));
		file_filter::filter_result result = file_filter::factories::create_result(argument);
		file_finder::recursive_scan(result, argument, impl, splitpath.first);

// 		file_finder::recursive_scan<file_finder::get_size>(splitpath.first, splitpath.second, -1, -1, sizeFinder, &errors, debug);
// 		if (errors.has_error()) {
// 			if (show_errors_)
// 				message = errors.get_error();
// 			else
// 				message = _T("Check contains error. Check log for details (or enable show_errors in nsc.ini)");
// 			return NSCAPI::returnUNKNOWN;
// 		}
		path.setDefault(tmpObject);
		path.perfData = bPerfData;

		checkHolders::disk_size_type size = impl->get_size();
		path.runCheck(size, returnCode, message, perf);
	}
	if (message.empty())
		message = _T("OK all file sizes are within bounds.");
	else
		message = NSCHelper::translateReturn(returnCode) + _T(": ") + message;
	return returnCode;
}



NSCAPI::nagiosReturn CheckDisk::getFileAge(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsUInteger> CheckFileContainer;
	if (stl_args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	std::wstring format = _T("%Y years %m mon %d days %H hours %M min %S sec");
	std::wstring path;
	bool debug = false;
	MAP_OPTIONS_BEGIN(stl_args)
		MAP_OPTIONS_STR(_T("path"), path)
		MAP_OPTIONS_STR(_T("date"), format)
		MAP_OPTIONS_BOOL_TRUE(_T("debug"), debug)
		MAP_OPTIONS_FALLBACK(format)
	MAP_OPTIONS_END()

	if (path.empty()) {
		message = _T("ERROR: no file specified.");
		return NSCAPI::returnUNKNOWN;
	}

	file_filter::filter_obj info = file_filter::filter_obj::get(path);

	if (!info.has_error()) {
		if (show_errors_)
			message = _T("0&") + info.get_error();
		else
			message = _T("0&Check contains error. Check log for details (or enable show_errors in nsc.ini)");
		return NSCAPI::returnUNKNOWN;
	}
	time_t value = info.get_write();
	message = strEx::itos(value/60) + _T("&") + strEx::format_time_delta(value, format);
	return NSCAPI::returnOK;
}


typedef checkHolders::ExactBounds<checkHolders::NumericBounds<checkHolders::disk_size_type, checkHolders::disk_size_handler<checkHolders::disk_size_type> > > ExactBoundsDiscSize;


typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULong> ExactULongContainer;
typedef checkHolders::CheckContainer<ExactBoundsDiscSize> DiscSizeContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsTime> DateTimeContainer;

struct check_file_size : public checkHolders::check_proxy_container<file_filter::filter_obj, DiscSizeContainer> {
	check_file_size() { set_alias(_T("size")); }
	checkHolders::disk_size_type get_value(file_filter::filter_obj &value) {
		return value.ullSize;
	}
};
struct check_file_line_count : public checkHolders::check_proxy_container<file_filter::filter_obj, ExactULongContainer> {
	check_file_line_count() { set_alias(_T("line-count")); }
	unsigned long get_value(file_filter::filter_obj &value) {
		return value.get_line_count();
	}
};
struct check_file_dates : public checkHolders::check_proxy_container<file_filter::filter_obj, DateTimeContainer> {
	enum type_type {
		date_access, date_creation, date_written
	} ;
	type_type type_;
	check_file_dates(type_type type) : type_(type) 
	{ 
		if (type_ == date_creation)
			set_alias(_T("creation")); 
		else if (type_ == date_access)
			set_alias(_T("access")); 
		else if (type_ == date_written)
			set_alias(_T("written")); 
		else
			set_alias(_T("unknown date type")); 
	}
	unsigned long long get_value(file_filter::filter_obj &value) {
		if (type_ == date_creation)
			return value.ullCreationTime;
		if (type_ == date_access)
			return value.ullLastAccessTime;
		if (type_ == date_written)
			return value.ullLastWriteTime;
		return -1;
	}
};

typedef checkHolders::check_multi_container<file_filter::filter_obj> check_file_multi;
struct check_file_factories {
	static checkHolders::check_proxy_interface<file_filter::filter_obj>* size() {
		return new check_file_size();
	}
	static checkHolders::check_proxy_interface<file_filter::filter_obj>* line_count() {
		return new check_file_line_count();
	}
	static checkHolders::check_proxy_interface<file_filter::filter_obj>* access() {
		return new check_file_dates(check_file_dates::date_access);
	}
	static checkHolders::check_proxy_interface<file_filter::filter_obj>* creation() {
		return new check_file_dates(check_file_dates::date_creation);
	}
	static checkHolders::check_proxy_interface<file_filter::filter_obj>* written() {
		return new check_file_dates(check_file_dates::date_written);
	}
};

#define MAP_FACTORY_PB(value, obj) \
		else if ((p__.first == _T("check")) && (p__.second == ##value)) { checker.add_check(check_file_factories::obj()); }


NSCAPI::nagiosReturn CheckDisk::CheckSingleFile(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	check_file_multi checker;
	typedef std::pair<int,file_finder::filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	if (stl_args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	std::list<std::wstring> files;
	unsigned int truncate = 0;
	std::wstring syntax = _T("%filename%");
	std::wstring alias;
	bool bPerfData = true;

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			//MAP_OPTIONS_NUMERIC_ALL(query, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), syntax)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_PUSH(_T("file"), files)
			MAP_OPTIONS_EXACT_NUMERIC_ALL_MULTI(checker, _T(""))
			MAP_FACTORY_PB(_T("size"), size)
			MAP_FACTORY_PB(_T("line-count"), line_count)
			MAP_FACTORY_PB(_T("creation"), creation)
			MAP_FACTORY_PB(_T("access"), access)
			MAP_FACTORY_PB(_T("written"), written)
			/*
			MAP_FILTER(_T("creation"), creation)
			MAP_FILTER(_T("written"), written)
			MAP_FILTER(_T("accessed"), accessed)
			MAP_FILTER(_T("version"), version)
			MAP_FILTER(_T("line-count"), line_count)
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
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	unsigned long long nowi64 = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	//finder.syntax = syntax;
	for (std::list<std::wstring>::const_iterator pit = files.begin(); pit != files.end(); ++pit) {
		file_filter::filter_obj info = file_filter::filter_obj::get(nowi64, *pit);
		checker.alias = info.render(syntax);
		checker.runCheck(info, returnCode, message, perf);
	}
	if ((truncate > 0) && (message.length() > (truncate-4))) {
		message = message.substr(0, truncate-4) + _T("...");
		perf = _T("");
	}
	if (message.empty())
		message = _T("CheckSingleFile ok");
	return returnCode;
}


NSCAPI::nagiosReturn CheckDisk::CheckFiles(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
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

			MAP_OPTIONS_STR(_T("filter"), args->filter)

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
	file_filter::filter_engine impl = file_filter::factories::create_engine(args);
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
			if (show_errors_)
				message = args->error->get_error();
			else
				message = _T("Check contains error. Check log for details (or enable show_errors in nsc.ini)");
			return NSCAPI::returnUNKNOWN;
		}
	}
	message = result->render(masterSyntax, returnCode);
	if (!alias.empty())
		query.alias = alias;
	else
		query.alias = _T("found files");
	unsigned int count = result->get_match_count();
	query.runCheck(count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4))) {
		message = message.substr(0, truncate-4) + _T("...");
		//perf = _T("");
	}
	if (message.empty())
		message = _T("CheckFile ok");
	return returnCode;
}



NSCAPI::nagiosReturn CheckDisk::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("CheckFileSize")) {
		return CheckFileSize(argLen, char_args, msg, perf);
	} else if (command == _T("CheckDriveSize")) {
		return CheckDriveSize(argLen, char_args, msg, perf);
 	} else if (command == _T("CheckFiles")) {
 		return CheckFiles(argLen, char_args, msg, perf);
	} else if (command == _T("OldCheckFile")) {
		return OldCheckDisk::CheckFile(argLen, char_args, msg, perf, show_errors_);
	} else if (command == _T("CheckFile2")) {
		return OldCheckDisk::CheckFile2(argLen, char_args, msg, perf, show_errors_);
	} else if (command == _T("CheckSingleFile")) {
		return CheckSingleFile(argLen, char_args, msg, perf);
	} else if (command == _T("getFileAge")) {
		return getFileAge(argLen, char_args, msg, perf);
	}	
	return NSCAPI::returnIgnored;
}


NSC_WRAPPERS_MAIN_DEF(gCheckDisk);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckDisk);
