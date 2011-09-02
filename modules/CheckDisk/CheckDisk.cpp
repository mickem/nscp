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
#include "filter.hpp"
#include <char_buffer.hpp>
#include <settings/client/settings_client.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

namespace sh = nscapi::settings_helper;

CheckDisk::CheckDisk() : show_errors_(false) {
}
CheckDisk::~CheckDisk() {
}


bool CheckDisk::loadModule() {
	return false;
}

bool CheckDisk::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		register_command(_T("CheckFileSize"), _T("Check or directory a file and verify its size."));
		register_command(_T("CheckDriveSize"), _T("Check the size (free-space) of a drive or volume."));
		register_command(_T("CheckFile2"), _T("(deprecated) Check various aspects of a file and/or folder."));
		register_command(_T("CheckFiles"), _T("Check various aspects of a file and/or folder."));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NRPE"), alias, _T("server"));

		settings.alias().add_path_to_settings()
			(_T("NRPE SERVER SECTION"), _T("Section for NRPE (NRPEListener.dll) (check_nrpe) protocol options."))
			;

		settings.alias().add_key_to_settings()
			(_T("show errors"), sh::bool_key(&show_errors_, false),
			_T("SHOW ERRORS"), _T(""))
			;
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
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



class volume_helper {

	typedef HANDLE (WINAPI *typeFindFirstVolumeW)( __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef BOOL (WINAPI *typeFindNextVolumeW)( __inout HANDLE hFindVolume, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength);
	typedef HANDLE (WINAPI *typeFindFirstVolumeMountPointW)( __in LPCWSTR lpszRootPathName, __out_ecount(cchBufferLength) LPWSTR lpszVolumeMountPoint, __in DWORD cchBufferLength );
	typedef BOOL (WINAPI *typeGetVolumeNameForVolumeMountPointW)( __in LPCWSTR lpszVolumeMountPoint, __out_ecount(cchBufferLength) LPWSTR lpszVolumeName, __in DWORD cchBufferLength );
	typeFindFirstVolumeW ptrFindFirstVolumeW;
	typeFindNextVolumeW ptrFindNextVolumeW;
	typeFindFirstVolumeMountPointW ptrFindFirstVolumeMountPointW;
	typeGetVolumeNameForVolumeMountPointW ptrGetVolumeNameForVolumeMountPointW;
	HMODULE hLib;

public:
	typedef std::map<std::wstring,std::wstring> map_type;

public:
	volume_helper() : ptrFindFirstVolumeW(NULL) {
		hLib = ::LoadLibrary(_TEXT("KERNEL32"));
		if (hLib) {
			// Find PSAPI functions
			ptrFindFirstVolumeW = (typeFindFirstVolumeW)::GetProcAddress(hLib, "FindFirstVolumeW");
			ptrFindNextVolumeW = (typeFindNextVolumeW)::GetProcAddress(hLib, "FindNextVolumeW");
			ptrFindFirstVolumeMountPointW = (typeFindFirstVolumeMountPointW)::GetProcAddress(hLib, "FindFirstVolumeMountPointW");
			ptrGetVolumeNameForVolumeMountPointW = (typeGetVolumeNameForVolumeMountPointW)::GetProcAddress(hLib, "GetVolumeNameForVolumeMountPointW");
		}
	}

	~volume_helper() {

	}

	HANDLE FindFirstVolume(std::wstring &volume) {
		if (ptrFindFirstVolumeW == NULL)
			return INVALID_HANDLE_VALUE;
		tchar_buffer  buffer(1024);
		HANDLE h = ptrFindFirstVolumeW(buffer.unsafe_get_buffer(), buffer.length());
		if (h != INVALID_HANDLE_VALUE)
			volume = buffer.unsafe_get_buffer();
		return h;
	}
	BOOL FindNextVolume(HANDLE hVolume, std::wstring &volume) {
		if (ptrFindFirstVolumeW == NULL || hVolume == INVALID_HANDLE_VALUE)
			return FALSE;
		tchar_buffer  buffer(1024);
		BOOL r = ptrFindNextVolumeW(hVolume, buffer.unsafe_get_buffer(), buffer.length());
		if (r)
			volume = buffer.unsafe_get_buffer();
		return r;
	}

	void getVolumeInformation(std::wstring volume, std::wstring &name) {
		tchar_buffer volumeName(1024);
		tchar_buffer fileSysName(1024);
		DWORD maximumComponentLength, fileSystemFlags;

		if (!GetVolumeInformation(volume.c_str(), volumeName.unsafe_get_buffer(), volumeName.length(), 
			NULL, &maximumComponentLength, &fileSystemFlags, fileSysName.unsafe_get_buffer(), fileSysName.length())) {
				NSC_LOG_ERROR_STD(_T("Failed to get volume information: ") + volume);
		} else {
			name = volumeName.unsafe_get_buffer();
		}
	}


	bool GetVolumeNameForVolumeMountPoint(std::wstring volumeMountPoint, std::wstring &volumeName) {
		tchar_buffer buffer(1024);
		if (ptrGetVolumeNameForVolumeMountPointW(volumeMountPoint.c_str(), buffer.unsafe_get_buffer(), buffer.length())) {
			volumeName = buffer;
			return true;
		}
		return false;
	}
	std::wstring GetVolumeNameForVolumeMountPoint(std::wstring volumeMountPoint) {
		std::wstring volumeName;
		GetVolumeNameForVolumeMountPoint(volumeMountPoint, volumeName);
		return volumeName;
	}

	map_type get_volumes(map_type alias) {
		map_type ret;
		std::wstring volume;
		HANDLE hVol = FindFirstVolume(volume);
		if (hVol == INVALID_HANDLE_VALUE) {
			NSC_LOG_ERROR_STD(_T("Failed to enumerate volumes"));
			return ret;
		}
		BOOL bFlag = TRUE;
		while (bFlag) {
			map_type::iterator it = alias.find(volume);
			if (it != alias.end())
				ret[volume] = (*it).second;
			else
				ret[volume] = get_title(volume);
			bFlag = FindNextVolume(hVol, volume);
		}
		return ret;
	}

	std::wstring get_title(std::wstring volume) {
		std::wstring title;
		getVolumeInformation(volume, title);
		return title;
	}


};




NSCAPI::nagiosReturn CheckDisk::CheckDriveSize(std::list<std::wstring> args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
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
	bool ignore_unreadable = false;
	float magic = 0;

	MAP_OPTIONS_BEGIN(args)
		MAP_OPTIONS_STR_AND(_T("Drive"), tmpObject.data, drives.push_back(tmpObject))
		MAP_OPTIONS_DISK_ALL(tmpObject, _T(""), _T("Free"), _T("Used"))
		MAP_OPTIONS_SHOWALL(tmpObject)
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterFixed, _T("FIXED"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterCDROM, _T("CDROM"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterRemovable, _T("REMOVABLE"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterRemote, _T("REMOTE"))
		MAP_OPTIONS_BOOL_VALUE(_T("FilterType"), bFilterNoRootDir, _T("NO_ROOT_DIR"))
		MAP_OPTIONS_BOOL_TRUE(_T("ignore-unreadable"), ignore_unreadable)
		MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
		MAP_OPTIONS_BOOL_TRUE(NSCLIENT, bNSClient)
		//MAP_OPTIONS_BOOL_TRUE(CHECK_ALL, bCheckAll)
		MAP_OPTIONS_STR(CHECK_ALL, strCheckAll)
		MAP_OPTIONS_DOUBLE(_T("magic"), magic)
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
		volume_helper helper;
		volume_helper::map_type volume_alias;

		DWORD bufSize = GetLogicalDriveStrings(0, NULL)+5;
		TCHAR *buffer = new TCHAR[bufSize+10];
		if (GetLogicalDriveStrings(bufSize, buffer)>0) {
			while (buffer[0] != 0) {
				std::wstring drv = buffer;
				volume_alias[helper.GetVolumeNameForVolumeMountPoint(drv)] = drv;
				buffer = &buffer[drv.size()];
				buffer++;
			}
		} else {
			NSC_LOG_ERROR_STD(_T("Failed to get buffer size: ") + error::lookup::last_error());
		}

		volume_helper::map_type volumes = helper.get_volumes(volume_alias);
		BOOST_FOREACH(volume_helper::map_type::value_type v, volumes) {
			UINT drvType = GetDriveType(v.first.c_str());
				if ( 
					((!bFilter)&&(drvType == DRIVE_FIXED))  ||
					((bFilter)&&(bFilterFixed)&&(drvType==DRIVE_FIXED)) ||
					((bFilter)&&(bFilterCDROM)&&(drvType==DRIVE_CDROM)) ||
					((bFilter)&&(bFilterRemote)&&(drvType==DRIVE_REMOTE)) ||
					((bFilter)&&(bFilterRemovable)&&(drvType==DRIVE_REMOVABLE)) ||
					((bFilter)&&(bFilterNoRootDir)&&(drvType==DRIVE_NO_ROOT_DIR)) 
					)
					drives.push_back(DriveContainer(v.first, v.second, tmpObject.warn, tmpObject.crit));
				else
				NSC_DEBUG_MSG_STD(_T("Ignoring drive: ") + v.second);
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
		std::wstring error;
			if (!GetDiskFreeSpaceEx(drive.data.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
				DWORD err = GetLastError();
				if (!ignore_unreadable || err != ERROR_ACCESS_DENIED) {
					message = _T("CRITICAL: Could not get free space for: ") + drive.getAlias() + _T(" ") + drive.data + _T(" reason: ") + error::lookup::last_error(err);
				return NSCAPI::returnCRIT;
			}
			drive.setDefault(tmpObject);
			error = drive.getAlias() + _T(": unreadable");
			freeBytesAvailableToCaller.QuadPart = 0;
			totalNumberOfFreeBytes.QuadPart = 0;
			totalNumberOfBytes.QuadPart = 0;
		}

		if (bNSClient) {
			if (!message.empty())
				message += _T("&");
			message += strEx::itos(totalNumberOfFreeBytes.QuadPart);
			message += _T("&");
			message += strEx::itos(totalNumberOfBytes.QuadPart);
		} else {
			if (error.empty()) {
				checkHolders::PercentageValueType<checkHolders::disk_size_type, checkHolders::disk_size_type> value;
				std::wstring tstr;
				value.value = totalNumberOfBytes.QuadPart-totalNumberOfFreeBytes.QuadPart;
				value.total = totalNumberOfBytes.QuadPart;
				drive.setDefault(tmpObject);
				drive.set_magic(magic);
				drive.runCheck(value, returnCode, message, perf);
			} else {
				strEx::append_list(message, error, _T(", "));
			}
		}
	}
	if (message.empty())
		message = _T("OK: All drives within bounds.");
	else if (!bNSClient)
		message = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + message;
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



/*
NSCAPI::nagiosReturn CheckDisk::CheckFileSize(std::list<std::wstring> args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
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
		MAP_OPTIONS_STR(_T("MaxWarn"), tmpObject.warn.max_)
		MAP_OPTIONS_STR(_T("MinWarn"), tmpObject.warn.min_)
		MAP_OPTIONS_STR(_T("MaxCrit"), tmpObject.crit.max_)
		MAP_OPTIONS_STR(_T("MinCrit"), tmpObject.crit.min_)
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

		file_filter::filter_argument argument = file_filter::factories::create_argument(splitpath.second, -1, _T(""), DATE_FORMAT);
		file_filter::filter_result result = file_filter::factories::create_result(argument);
		file_finder::recursive_scan(result, argument, impl, splitpath.first);

		path.setDefault(tmpObject);
		path.perfData = bPerfData;

		checkHolders::disk_size_type size = impl->get_size();
		path.runCheck(size, returnCode, message, perf);
	}
	if (message.empty())
		message = _T("OK all file sizes are within bounds.");
	else
		message = nscapi::plugin_helper::translateReturn(returnCode) + _T(": ") + message;
	return returnCode;
}



NSCAPI::nagiosReturn CheckDisk::getFileAge(std::list<std::wstring> args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsUInteger> CheckFileContainer;
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	std::wstring format = _T("%Y years %m mon %d days %H hours %M min %S sec");
	std::wstring path;
	bool debug = false;
	MAP_OPTIONS_BEGIN(args)
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
*/

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

/*
NSCAPI::nagiosReturn CheckDisk::CheckSingleFile(std::list<std::wstring> args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	check_file_multi checker;
	typedef std::pair<int,file_finder::filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
}
	std::list<std::wstring> files;
	unsigned int truncate = 0;
	std::wstring syntax = _T("%filename%");
	std::wstring datesyntax = DATE_FORMAT;
	std::wstring alias;
	bool bPerfData = true;

	try {
		MAP_OPTIONS_BEGIN(args)
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
/ *
			MAP_FILTER(_T("creation"), creation)
			MAP_FILTER(_T("written"), written)
			MAP_FILTER(_T("accessed"), accessed)
			MAP_FILTER(_T("version"), version)
			MAP_FILTER(_T("line-count"), line_count)
			* /
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
		checker.alias = info.render(syntax, datesyntax);
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
*/

NSCAPI::nagiosReturn CheckDisk::CheckFiles(std::list<std::wstring> args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsULongInteger> CheckFileQuery1Container;
	typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULongInteger> CheckFileQuery2Container;
// 	typedef std::pair<int,file_finder::filter> filteritem_type;
// 	typedef std::list<filteritem_type> filterlist_type;
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	PathContainer tmpObject;
	std::list<std::wstring> paths;
	unsigned int truncate = 0;
	CheckFileQuery1Container query1;
	CheckFileQuery2Container query2;
	std::wstring masterSyntax = _T("%list%");
	std::wstring alias;
	bool bPerfData = true;
	bool ignoreError = false;

	file_filter::filter_argument fargs = file_filter::factories::create_argument(_T("*.*"), -1, _T("%filename%"), DATE_FORMAT);

	try {
		MAP_OPTIONS_BEGIN(args)
			MAP_OPTIONS_NUMERIC_ALL(query1, _T(""))
			MAP_OPTIONS_EXACT_NUMERIC_ALL(query2, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), fargs->syntax)
			MAP_OPTIONS_STR(_T("date-syntax"), fargs->date_syntax)
			MAP_OPTIONS_STR(_T("master-syntax"), masterSyntax)
			MAP_OPTIONS_PUSH(_T("path"), paths)
			MAP_OPTIONS_STR(_T("pattern"), fargs->pattern)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_PUSH(_T("file"), paths)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), fargs->debug)
			MAP_OPTIONS_BOOL_TRUE(_T("ignore-errors"), ignoreError)
			MAP_OPTIONS_STR2INT(_T("max-dir-depth"), fargs->max_level)
			MAP_OPTIONS_BOOL_EX(_T("filter"), fargs->bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), fargs->bFilterAll, _T("all"), _T("any"))

			MAP_OPTIONS_STR(_T("filter"), fargs->filter)

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
	file_filter::filter_engine impl = file_filter::factories::create_engine(fargs);
	if (!impl) {
		message = _T("Missing filter argument");
		return NSCAPI::returnUNKNOWN;
	}
	if (!impl->validate(message))
		return NSCAPI::returnUNKNOWN;
	if (fargs->debug)
		NSC_DEBUG_MSG_STD(_T("NOW: ") + strEx::format_filetime(fargs->now));

	file_filter::filter_result result = file_filter::factories::create_result(fargs);
	for (std::list<std::wstring>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {

		file_finder::recursive_scan(result, fargs, impl, *pit);
		if (!ignoreError && fargs->error->has_error()) {
			if (show_errors_)
				message = fargs->error->get_error();
			else
				message = _T("Check contains error. Check log for details (or enable show_errors in nsc.ini)");
			return NSCAPI::returnUNKNOWN;
		}
	}
	message = result->render(masterSyntax, returnCode);

	if (!bPerfData) {
		query1.perfData = false;
		query2.perfData = false;
	}

	if (query1.alias.empty())
		query1.alias = _T("found files");
	if (query2.alias.empty())
		query2.alias = _T("found files");
	unsigned long count = result->get_match_count();
	if (query1.hasBounds())
		query1.runCheck(count, returnCode, message, perf);
	else if (query2.hasBounds())
		query2.runCheck(count, returnCode, message, perf);
	else {
		message = _T("No bounds specified!");
		return NSCAPI::returnUNKNOWN;
	}
	if ((truncate > 0) && (message.length() > (truncate-4))) {
		message = message.substr(0, truncate-4) + _T("...");
		//perf = _T("");
	}
	if (message.empty())
		message = _T("CheckFile ok");
	return returnCode;
}



NSCAPI::nagiosReturn CheckDisk::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("checkfiles"))
		return CheckFiles(arguments, message, perf);
// 	else if (command == _T("checkfilesize"))
// 		return CheckFileSize(arguments, message, perf);
	else if (command == _T("checkdrivesize"))
		return CheckDriveSize(arguments, message, perf);
// 	else if (command == _T("checksinglefile"))
// 		return CheckSingleFile(arguments, message, perf);
// 	else if (command == _T("getfileage"))
// 		return getFileAge(arguments, message, perf);
	return NSCAPI::returnIgnored;
}


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(CheckDisk);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
