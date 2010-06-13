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

bool is_directory(DWORD dwAttr) {
	return ((dwAttr != INVALID_FILE_ATTRIBUTES) && ((dwAttr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
}

bool CheckDisk::loadModule(NSCAPI::moduleLoadMode mode) {

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

class error_reporter {
public:
	virtual void report_error(std::wstring error) = 0;
	virtual void report_warning(std::wstring error) = 0;
	virtual bool has_error() = 0;
	virtual std::wstring get_error() = 0;
};



struct file_finder_data {
	file_finder_data(const WIN32_FIND_DATA wfd_, const std::wstring path_, error_reporter *errors_) : wfd(wfd_), path(path_), errors(errors_) {}
	const WIN32_FIND_DATA wfd;
	const std::wstring path;
	error_reporter *errors;
};
typedef std::unary_function<const file_finder_data&, bool> baseFinderFunction;

struct get_size : public baseFinderFunction
{
	get_size() : size(0) { }
	result_type operator()(argument_type ffd) {
		if (!file_helpers::checks::is_directory(ffd.wfd.dwFileAttributes)) {
			size += (ffd.wfd.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)ffd.wfd.nFileSizeLow;
		}
		return true;
	}
	inline unsigned long long getSize() {
		return size;
	}
	inline void setError(error_reporter *errors, std::wstring msg) {
		if (errors != NULL)
			errors->report_error(msg);
	}
private:  
	unsigned long long size;
};

template <class finder_function>
void recursive_scan(std::wstring dir, std::wstring pattern, int current_level, int max_level, finder_function & f, error_reporter * errors) {
	if ((max_level != -1) && (current_level > max_level))
		return;
	WIN32_FIND_DATA wfd;

	DWORD fileAttr = GetFileAttributes(dir.c_str());
	NSC_DEBUG_MSG_STD(_T("Input is: ") + dir + _T(" / ") + strEx::ihextos(fileAttr));

	if (!file_helpers::checks::is_directory(fileAttr)) {
		NSC_DEBUG_MSG_STD(_T("Found a file dont do recursive scan: ") + dir);
		// It is a file check it an return (dont check recursivly)
		pattern_type single_path = split_path_ex(dir);
		NSC_DEBUG_MSG_STD(_T("Path is: ") + single_path.first);
		HANDLE hFind = FindFirstFile(dir.c_str(), &wfd);
		if (hFind != INVALID_HANDLE_VALUE) {
			f(file_finder_data(wfd, single_path.first, errors));
			FindClose(hFind);
		}
		return;
	}
	std::wstring file_pattern = dir + _T("\\") + pattern;
	NSC_DEBUG_MSG_STD(_T("File pattern: ") + file_pattern);
	HANDLE hFind = FindFirstFile(file_pattern.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!f(file_finder_data(wfd, dir, errors)))
				break;
		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	std::wstring dir_pattern = dir + _T("\\*.*");
	NSC_DEBUG_MSG_STD(_T("File pattern: ") + dir_pattern);
	hFind = FindFirstFile(dir_pattern.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (file_helpers::checks::is_directory(wfd.dwFileAttributes)) {
				if ( (wcscmp(wfd.cFileName, _T(".")) != 0) && (wcscmp(wfd.cFileName, _T("..")) != 0) )
					recursive_scan<finder_function>(dir + _T("\\") + wfd.cFileName, pattern, current_level+1, max_level, f, errors, debug);
			}
		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
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
4


NSCAPI::nagiosReturn CheckDisk::CheckFileSize(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	bool bPerfData = true;
	bool debug = false;
	if (args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	PathContainer tmpObject;
	std::list<PathContainer> paths;

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

	for (std::list<PathContainer>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {
		PathContainer path = (*pit);
		std::wstring tstr;
		std::wstring sName = path.getAlias();
		get_size sizeFinder;
		NSC_error errors;
		pattern_type splitpath = split_pattern(path.data);
		recursive_scan<get_size>(splitpath.first, splitpath.second, -1, -1, sizeFinder, &errors, debug);
		if (sizeFinder.hasError()) {
			message = _T("File not found check log for details");
			return NSCAPI::returnUNKNOWN;
		}
		path.setDefault(tmpObject);
		path.perfData = bPerfData;

		checkHolders::disk_size_type size = sizeFinder.getSize();
		path.runCheck(size, returnCode, message, perf);
	}
	if (message.empty())
		message = _T("OK all file sizes are within bounds.");
	else
		message = NSCHelper::translateReturn(returnCode) + _T(": ") + message;
	return returnCode;
}


struct file_info {

	std::wstring error;
	//bool has_error;

	static file_info get(__int64 now, std::wstring path, std::wstring file) {
		return get_2(now, path, file);
	}
	static file_info get(__int64 now, file_finder_data data) {
		return file_info(now, data.wfd, data.path, data.wfd.cFileName);
	}

	static file_info get_2(__int64 now, std::wstring path, std::wstring file) {
		WIN32_FILE_ATTRIBUTE_DATA data;
		if (!GetFileAttributesEx((path + _T("\\") + file).c_str(), GetFileExInfoStandard, reinterpret_cast<LPVOID>(&data))) {
			file_info ret;
			ret.error = _T("Could not open file (2) ") + path + _T("\\") + file + _T(": ") + error::lookup::last_error();
			return ret;
		}
		return file_info(now, data, path, file);
	}
	static file_info get_1(__int64 now, std::wstring path, std::wstring file) {
		HANDLE hFile = CreateFile((path + _T("\\") + file).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			file_info ret;
			ret.error = _T("Could not open file (1) ") + path + _T("\\") + file + _T(": ") + error::lookup::last_error();
			return ret;
		}
		BY_HANDLE_FILE_INFORMATION _info;
		GetFileInformationByHandle(hFile, &_info);
		CloseHandle(hFile);
		return file_info(now, _info, path, file);
	}

	file_info() 
		: ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, ullNow(0)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{}
	file_info(__int64 now, const WIN32_FILE_ATTRIBUTE_DATA info, std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, ullNow(now)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
		ullSize = ((info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow);
		ullCreationTime = ((info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime);
		ullLastAccessTime = ((info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime);
		ullLastWriteTime = ((info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime);
	};
	file_info(__int64 now, const BY_HANDLE_FILE_INFORMATION info, std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, ullNow(now)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
		ullSize = ((info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow);
		ullCreationTime = ((info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime);
		ullLastAccessTime = ((info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime);
		ullLastWriteTime = ((info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime);
	};
	file_info(__int64 now, const WIN32_FIND_DATA info, std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, ullNow(now)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
	bool has_error;

	static file_info get(std::wstring path, std::wstring file) {
		return get_2(path, file);
	}
	static file_info get(file_finder_data data) {
		return file_info(data.wfd, data.path, data.wfd.cFileName);
	}

	static file_info get_2(std::wstring path, std::wstring file) {
		WIN32_FILE_ATTRIBUTE_DATA data;
		if (!GetFileAttributesEx((path + _T("\\") + file).c_str(), GetFileExInfoStandard, reinterpret_cast<LPVOID>(&data))) {
			file_info ret;
			ret.error = _T("Could not open file (2) ") + path + _T("\\") + file + _T(": ") + error::lookup::last_error();
			return ret;
		}
		return file_info(data, path, file);
	}
	static file_info get_1(std::wstring path, std::wstring file) {
		HANDLE hFile = CreateFile((path + _T("\\") + file).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			file_info ret;
			ret.error = _T("Could not open file (1) ") + path + _T("\\") + file + _T(": ") + error::lookup::last_error();
			return ret;
		}
		BY_HANDLE_FILE_INFORMATION _info;
		GetFileInformationByHandle(hFile, &_info);
		CloseHandle(hFile);
		return file_info(_info, path, file);
	}

	file_info() 
		: ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{}
	file_info(const WIN32_FILE_ATTRIBUTE_DATA info, std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
		ullSize = ((info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow);
		ullCreationTime = ((info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime);
		ullLastAccessTime = ((info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime);
		ullLastWriteTime = ((info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime);
	};
	file_info(const BY_HANDLE_FILE_INFORMATION info, std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
		ullSize = ((info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow);
		ullCreationTime = ((info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime);
		ullLastAccessTime = ((info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime);
		ullLastWriteTime = ((info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime);
	};
	file_info(const WIN32_FIND_DATA info, std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
		ullSize = ((info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow);
		ullCreationTime = ((info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime);
		ullLastAccessTime = ((info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime);
		ullLastWriteTime = ((info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime);
	};
	file_info(__int64 now, std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, ullNow(now)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
	};
	file_info(std::wstring path_, std::wstring filename_) 
		: path(path_)
		, filename(filename_)
		, ullCreationTime(0)
		, ullLastAccessTime(0)
		, ullLastWriteTime(0)
		, ullSize(0)
		, cached_version(false, _T("")) 
		, cached_count(false, 0)
	{
	};

	unsigned long long ullSize;
	__int64 ullCreationTime;
	__int64 ullLastAccessTime;
	__int64 ullLastWriteTime;
	__int64 ullNow;
	std::wstring filename;
	std::wstring path;
	std::pair<bool,std::wstring> cached_version;
	std::pair<bool,unsigned long> cached_count;

	static const __int64 MSECS_TO_100NS = 10000;

	__int64 get_creation() {
		return (ullNow-ullCreationTime)/MSECS_TO_100NS;
	}
	__int64 get_access() {
		return (ullNow-ullLastAccessTime)/MSECS_TO_100NS;
	}
	__int64 get_write() {
		return (ullNow-ullLastWriteTime)/MSECS_TO_100NS;
	}
	std::wstring render(std::wstring syntax) {
		strEx::replace(syntax, _T("%path%"), path);
		strEx::replace(syntax, _T("%filename%"), filename);
		strEx::replace(syntax, _T("%creation%"), strEx::format_filetime(ullCreationTime, DATE_FORMAT));
		strEx::replace(syntax, _T("%access%"), strEx::format_filetime(ullLastAccessTime, DATE_FORMAT));
		strEx::replace(syntax, _T("%write%"), strEx::format_filetime(ullLastWriteTime, DATE_FORMAT));
		strEx::replace(syntax, _T("%creation-raw%"), strEx::itos(ullCreationTime));
		strEx::replace(syntax, _T("%access-raw%"), strEx::itos(ullLastAccessTime));
		strEx::replace(syntax, _T("%write-raw%"), strEx::itos(ullLastWriteTime));
		strEx::replace(syntax, _T("%now-raw%"), strEx::itos(ullNow));
/*
		strEx::replace(syntax, _T("%creation-d%"), strEx::format_filetime(ullCreationTime, DATE_FORMAT));
		strEx::replace(syntax, _T("%access-d%"), strEx::format_filetime(ullLastAccessTime, DATE_FORMAT));
		strEx::replace(syntax, _T("%write-d%"), strEx::format_filetime(ullLastWriteTime, DATE_FORMAT));
*/
		strEx::replace(syntax, _T("%size%"), strEx::itos_as_BKMG(ullSize));
		if (cached_version.first)
			strEx::replace(syntax, _T("%version%"), cached_version.second);
		if (cached_count.first)
			strEx::replace(syntax, _T("%line-count%"), strEx::itos(cached_count.second));
		return syntax;
	}

	std::wstring get_version() {
		if (cached_version.first)
			return cached_version.second;
		std::wstring fullpath = path+_T("\\")+filename;

		DWORD dwDummy;
		DWORD dwFVISize = GetFileVersionInfoSize(fullpath.c_str(),&dwDummy);
		LPBYTE lpVersionInfo = new BYTE[dwFVISize+1];
		GetFileVersionInfo(fullpath.c_str(),0,dwFVISize,lpVersionInfo);
		UINT uLen;
		VS_FIXEDFILEINFO *lpFfi;
		VerQueryValue( lpVersionInfo , _T("\\") , (LPVOID *)&lpFfi , &uLen );
		DWORD dwFileVersionMS = lpFfi->dwFileVersionMS;
		DWORD dwFileVersionLS = lpFfi->dwFileVersionLS;
		delete [] lpVersionInfo;
		DWORD dwLeftMost = HIWORD(dwFileVersionMS);
		DWORD dwSecondLeft = LOWORD(dwFileVersionMS);
		DWORD dwSecondRight = HIWORD(dwFileVersionLS);
		DWORD dwRightMost = LOWORD(dwFileVersionLS);
		cached_version.second = strEx::itos(dwLeftMost) + _T(".") +
			strEx::itos(dwSecondLeft) + _T(".") +
			strEx::itos(dwSecondRight) + _T(".") +
			strEx::itos(dwRightMost);
		cached_version.first = true;
		return cached_version.second;
	}

	unsigned long get_line_count() {
		if (cached_count.first)
			return cached_count.second;

		unsigned long count = 0;
		std::wstring fullpath = path+_T("\\")+filename;
		FILE * pFile = fopen(strEx::wstring_to_string(fullpath).c_str(),"r");;
		if (pFile==NULL) 
			return 0;
		char c;
		do {
			c = fgetc (pFile);
			if (c == '\r') {
				c = fgetc (pFile);
				count++;
			} else if (c == '\n') {
				c = fgetc (pFile);
				count++;
			}
		} while (c != EOF);
		fclose (pFile);
		cached_count.second = count;
		cached_count.first = true;
		return cached_count.second;
	}
};

struct file_container : public file_info {
	std::wstring error_;


	static file_container get(std::wstring file) {
		FILETIME now;
		GetSystemTimeAsFileTime(&now);
		unsigned __int64 nowi64 = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
		return get(file, nowi64);
	}

	static file_container get(std::wstring file, unsigned long long now) {

		BY_HANDLE_FILE_INFORMATION _info;

		HANDLE hFile = CreateFile(file.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			return file_container(now, file, _T("Could not open file: ") + file);
		}
		GetFileInformationByHandle(hFile, &_info);
		CloseHandle(hFile);
		file_container info(now, _info, file);
		//info.ullNow = now;
		return info;
	}


	file_container(__int64 now, const BY_HANDLE_FILE_INFORMATION info, std::wstring file) : file_info(now, info, file_helpers::meta::get_path(file), file_helpers::meta::get_filename(file)) {}
	file_container(__int64 now, std::wstring file, std::wstring error) : error_(error), file_info(now, file_helpers::meta::get_path(file), file_helpers::meta::get_filename(file)) {}

	bool has_errors() {
		return !error_.empty();
	}
	std::wstring get_error() {
		return error_;
	}

};
struct file_filter {
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
	bool matchFilter(file_info &value) const {
		if ((size.hasFilter())&&(size.matchFilter(value.ullSize)))
			return true;
		else if (creation.hasFilter()&&creation.matchFilter(value.get_creation()))
			return true;
		else if (accessed.hasFilter()&&accessed.matchFilter(value.get_access()))
			return true;
		else if (written.hasFilter()&&written.matchFilter(value.get_write()))
			return true;
		else if ((version.hasFilter())&&(version.matchFilter(value.get_version())))
			return true;
		else if ((line_count.hasFilter())&&(line_count.matchFilter(value.get_line_count())))
			return true;
		return false;
	}

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


struct find_first_file_info : public baseFinderFunction
{
	file_info info;
	__int64 now_;
//	std::wstring message;
	find_first_file_info() : now_(0) {
		FILETIME now;
		GetSystemTimeAsFileTime(&now);
		now_ = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	}
	result_type operator()(argument_type ffd) {
		if (file_helpers::checks::is_directory(ffd.wfd.dwFileAttributes))
			return true;

		file_info info = file_info::get(now_, ffd);
		file_info info = file_info::get(ffd);
		if (!info.error.empty()) {
			setError(ffd.errors, info.error);
			return false;
		}
		return false;
		/*
		BY_HANDLE_FILE_INFORMATION _info;

		HANDLE hFile = CreateFile((ffd.path + _T("\\") + ffd.wfd.cFileName).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			setError(ffd.errors, _T("Could not open file: ") + ffd.path + _T("\\") + ffd.wfd.cFileName + _T(": ") + error::lookup::last_error());
			return false;
		}
		GetFileInformationByHandle(hFile, &_info);
		CloseHandle(hFile);
		info = file_info(_info, ffd.path, ffd.wfd.cFileName);
		return false;
		*/
	}
	inline void setError(error_reporter *errors, std::wstring msg) {
		if (errors != NULL)
			errors->report_error(msg);
	}
};

struct file_filter_function : public baseFinderFunction
{
	std::list<file_filter> filter_chain;
	bool bFilterAll;
	bool bFilterIn;
	std::wstring message;
	std::wstring syntax;
	std::wstring alias;
	unsigned long long now;
	unsigned int hit_count;
	__int64 now_;

	file_filter_function() : now_(0), hit_count(0), bFilterIn(true), bFilterAll(true) {
		FILETIME now;
		GetSystemTimeAsFileTime(&now);
		now_ = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	}
	result_type operator()(argument_type ffd) {
		if (file_helpers::checks::is_directory(ffd.wfd.dwFileAttributes))
			return true;

		file_info info = file_info::get(ffd);
		if (!info.error.empty()) {
			setError(ffd.errors, info.error);
			return true;
		}
		/*
		BY_HANDLE_FILE_INFORMATION _info;

		HANDLE hFile = CreateFile((ffd.path + _T("\\") + ffd.wfd.cFileName).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			setError(ffd.errors, _T("Could not open file: ") + ffd.path + _T("\\") + ffd.wfd.cFileName + _T(": ") + error::lookup::last_error());
			return true;
		}
		GetFileInformationByHandle(hFile, &_info);
		CloseHandle(hFile);
		file_info info(_info, ffd.path, ffd.wfd.cFileName);
		info.ullNow = now;
		*/

		for (std::list<file_filter>::const_iterator cit3 = filter_chain.begin(); cit3 != filter_chain.end(); ++cit3 ) {
			bool bMatch = bFilterAll;
			bool bTmpMatched = (*cit3).matchFilter(info);
			if (bFilterAll) {
				if (!bTmpMatched) {
					bMatch = false;
					break;
				}
			} else {
				if (bTmpMatched) {
					bMatch = true;
					break;
				}
			}
			if ((bFilterIn&&bMatch)||(!bFilterIn&&!bMatch)) {
				strEx::append_list(message, info.render(syntax));
				if (alias.length() < 16)
					strEx::append_list(alias, info.filename);
				else
					strEx::append_list(alias, std::wstring(_T("...")));
				hit_count++;
			}
		}
		return true;
	}
	}
	inline void setError(error_reporter *errors, std::wstring msg) {
		if (errors != NULL)
			errors->report_error(msg);
		error = true;
	}
};



struct file_filter_function_ex : public baseFinderFunction
{
	static const int filter_plus = 1;
	static const int filter_minus = 2;
	static const int filter_normal = 3;

	typedef std::pair<int,file_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	filterlist_type filter_chain;
	bool bFilterAll;
	bool bFilterIn;
	bool error;
	bool debug_;
	std::wstring message;
	std::wstring syntax;
	//std::wstring alias;
	unsigned long long now;
	unsigned int hit_count;
	unsigned int file_count;
	std::wstring last_error;
	unsigned int error_count;

	file_filter_function_ex() : hit_count(0), file_count(0), error(false), debug_(false), bFilterIn(true), bFilterAll(true), error_count(0) {}
	result_type operator()(argument_type ffd) {
		if (is_directory(ffd.wfd.dwFileAttributes))
			return true;

		file_info info = file_info::get(ffd);
		if (!info.error.empty()) {
			setError(ffd.errors, info.error);
			return true;
		}
		/*
		BY_HANDLE_FILE_INFORMATION _info;

		HANDLE hFile = CreateFile((ffd.path + _T("\\") + ffd.wfd.cFileName).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			setError(ffd.errors, _T("Could not open file: ") + ffd.path + _T("\\") + ffd.wfd.cFileName + _T(": ") + error::lookup::last_error());
			return true;
		}
		GetFileInformationByHandle(hFile, &_info);
		CloseHandle(hFile);
		file_info info(_info, ffd.path, ffd.wfd.cFileName);
		info.ullNow = now;
		*/

		bool bMatch = !bFilterIn;
		for (filterlist_type::const_iterator cit3 = filter_chain.begin(); cit3 != filter_chain.end(); ++cit3 ) {
			bool bTmpMatched = (*cit3).second.matchFilter(info);
			int mode = (*cit3).first;

			if ((mode == filter_minus)&&(bTmpMatched)) {
				// a -<filter> hit so thrash item and bail out!
				if (debug_)
					NSC_DEBUG_MSG_STD(_T("Matched: - ") + (*cit3).second.getValue() + _T(" for: ") + info.render(syntax));
				bMatch = false;
				break;
			} else if ((mode == filter_plus)&&(!bTmpMatched)) {
				// a +<filter> missed hit so thrash item and bail out!
				if (debug_)
					NSC_DEBUG_MSG_STD(_T("Matched (missed): + ") + (*cit3).second.getValue() + _T(" for: ") + info.render(syntax));
				bMatch = false;
				break;
			} else if (bTmpMatched) {
				if (debug_)
					NSC_DEBUG_MSG_STD(_T("Matched: . (contiunue): ") + (*cit3).second.getValue() + _T(" for: ") + info.render(syntax));
				bMatch = true;
			}
		}

		//NSC_DEBUG_MSG_STD(_T("result: ") + strEx::itos(bFilterIn) + _T(" -- ") + strEx::itos(bMatch));
		if ((bFilterIn&&bMatch)||(!bFilterIn&&!bMatch)) {
			strEx::append_list(message, info.render(syntax));
			/*
			if (alias.length() < 16)
				strEx::append_list(alias, info.filename);
			else
				strEx::append_list(alias, std::wstring(_T("...")));
				*/
			hit_count++;
		}
		file_count++;
		return true;
	}
	inline const bool hasError() const {
		return error;
	}
	inline void setError(error_reporter *errors, std::wstring msg) {
		if (errors != NULL)
			errors->report_error(msg);
		last_error = msg;
		error_count++;
	}

	std::wstring render(std::wstring syntax) {
		strEx::replace(syntax, _T("%list%"), message);
		strEx::replace(syntax, _T("%matches%"), strEx::itos(hit_count));
		strEx::replace(syntax, _T("%files%"), strEx::itos(file_count));
		return syntax;
	}

	bool has_filter() {
		return !filter_chain.empty();
	}

};


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

	NSC_error errors;
	pattern_type splitpath = split_pattern(path);
	recursive_scan<find_first_file_info>(splitpath.first, splitpath.second, -1, -1, finder, &errors, debug);
	if (finder.hasError()) {
		message = _T("File not found (check log for details)");
		return NSCAPI::returnUNKNOWN;
	}
	FILETIME now_;
	GetSystemTimeAsFileTime(&now_);
	unsigned long long now = ((now_.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now_.dwLowDateTime);
	time_t value = (now-finder.info.ullLastWriteTime)/10000000;
	message = to_wstring(value/60) + _T("&") + strEx::format_time_delta(gmtime(&value), format);
	return NSCAPI::returnOK;
}


NSCAPI::nagiosReturn CheckDisk::CheckFile(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsUInteger> CheckFileContainer;
	if (stl_args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	file_filter_function finder;
	PathContainer tmpObject;
	std::list<std::wstring> paths;
	unsigned int truncate = 0;
	CheckFileContainer query;
	std::wstring syntax = _T("%filename%");
	std::wstring alias;
	bool bPerfData = true;
	unsigned int max_dir_depth = -1;
	bool debug = false;

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), syntax)
			MAP_OPTIONS_PUSH(_T("path"), paths)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_STR2INT(_T("max-dir-depth"), max_dir_depth)
			MAP_OPTIONS_PUSH(_T("file"), paths)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), debug)
			MAP_OPTIONS_BOOL_EX(_T("filter"), finder.bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), finder.bFilterAll, _T("all"), _T("any"))
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-size"), size, finder.filter_chain)
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-creation"), creation, finder.filter_chain)
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-written"), written, finder.filter_chain)
			MAP_OPTIONS_PUSH_WTYPE(file_filter, _T("filter-accessed"), accessed, finder.filter_chain)
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	}
	finder.syntax = syntax;
	NSC_error errors;
	for (std::list<std::wstring>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {
		pattern_type path = split_pattern(*pit);
		recursive_scan<file_filter_function>(path.first, path.second, 0, max_dir_depth, finder, &errors, debug);
		if (errors.has_error()) {
			if (show_errors_)
				message = errors.get_error();
			else
				message = _T("Check contains error. Check log for details (or enable show_errors in nsc.ini)");
			return NSCAPI::returnUNKNOWN;
		}
	}
	message = finder.message;
	if (!alias.empty())
		query.alias = alias;
	else
		query.alias = finder.alias;
	if (query.alias.empty())
		query.alias = _T("no files found");
	query.runCheck(finder.hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("CheckFile ok");
	return returnCode;
}

#define MAP_FILTER(value, obj) \
		else if (p__.first == _T("filter+"##value)) { file_filter filter; filter.obj = p__.second; \
			finder.filter_chain.push_back(filteritem_type(file_filter_function_ex::filter_plus, filter)); } \
		else if (p__.first == _T("filter-"##value)) { file_filter filter; filter.obj = p__.second; \
			finder.filter_chain.push_back(filteritem_type(file_filter_function_ex::filter_minus, filter)); } \
		else if (p__.first == _T("filter."##value)) { file_filter filter; filter.obj = p__.second; \
			finder.filter_chain.push_back(filteritem_type(file_filter_function_ex::filter_normal, filter)); }

NSCAPI::nagiosReturn CheckDisk::CheckFile2(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsUInteger> CheckFileContainer;
	typedef std::pair<int,file_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	if (stl_args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	file_filter_function_ex finder;
	PathContainer tmpObject;
	std::list<std::wstring> paths;
	unsigned int truncate = 0;
	CheckFileContainer query;
	std::wstring syntax = _T("%filename%");
	std::wstring masterSyntax = _T("%list%");
	std::wstring alias;
	std::wstring pattern = _T("*.*");
	bool bPerfData = true;
	int max_dir_depth = -1;
	bool debug = false;
	bool ignoreError = false;

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), syntax)
			MAP_OPTIONS_STR(_T("master-syntax"), masterSyntax)
			MAP_OPTIONS_PUSH(_T("path"), paths)
			MAP_OPTIONS_STR(_T("pattern"), pattern)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_PUSH(_T("file"), paths)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), debug)
			MAP_OPTIONS_BOOL_TRUE(_T("ignore-errors"), ignoreError)
			MAP_OPTIONS_STR2INT(_T("max-dir-depth"), max_dir_depth)
			MAP_OPTIONS_BOOL_EX(_T("filter"), finder.bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), finder.bFilterAll, _T("all"), _T("any"))
			/*
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
	finder.now = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	finder.syntax = syntax;
	NSC_error errors;
	for (std::list<std::wstring>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {
		pattern_type path = split_pattern(*pit);
		recursive_scan<file_filter_function>(path.first, path.second, 0, max_dir_depth, finder, &errors, debug);
		if (finder.hasError()) {
			message = _T("File not found: ") + (*pit) + _T(" check log for details.");
			return NSCAPI::returnUNKNOWN;
		}
	}
	message = finder.message;
	if (finder.error)
		return NSCAPI::returnUNKNOWN;
	if (!alias.empty())
		query.alias = alias;
	else
		query.alias = finder.alias;
	if (query.alias.empty())
		query.alias = _T("no files found");
	query.runCheck(finder.hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("CheckFile ok");
	return returnCode;
}

#define MAP_FILTER(value, obj) \
		else if (p__.first == _T("filter+"##value)) { file_filter filter; filter.obj = p__.second; \
			finder.filter_chain.push_back(filteritem_type(file_filter_function_ex::filter_plus, filter)); } \
		else if (p__.first == _T("filter-"##value)) { file_filter filter; filter.obj = p__.second; \
			finder.filter_chain.push_back(filteritem_type(file_filter_function_ex::filter_minus, filter)); } \
		else if (p__.first == _T("filter."##value)) { file_filter filter; filter.obj = p__.second; \
			finder.filter_chain.push_back(filteritem_type(file_filter_function_ex::filter_normal, filter)); }

NSCAPI::nagiosReturn CheckDisk::CheckFile2(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsUInteger> CheckFileContainer;
	typedef std::pair<int,file_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	if (stl_args.empty()) {
		message = _T("Missing argument(s).");
		return NSCAPI::returnUNKNOWN;
	}
	file_filter_function_ex finder;
	PathContainer tmpObject;
	std::list<std::wstring> paths;
	unsigned int truncate = 0;
	CheckFileContainer query;
	std::wstring syntax = _T("%filename%");
	std::wstring masterSyntax = _T("%list%");
	std::wstring alias;
	std::wstring pattern = _T("*.*");
	bool bPerfData = true;
	int max_dir_depth = -1;
	bool debug = false;
	bool ignoreError = false;

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_STR(_T("syntax"), syntax)
			MAP_OPTIONS_STR(_T("master-syntax"), masterSyntax)
			MAP_OPTIONS_PUSH(_T("path"), paths)
			MAP_OPTIONS_STR(_T("pattern"), pattern)
			MAP_OPTIONS_STR(_T("alias"), alias)
			MAP_OPTIONS_PUSH(_T("file"), paths)
			MAP_OPTIONS_BOOL_TRUE(_T("debug"), debug)
			MAP_OPTIONS_BOOL_TRUE(_T("ignore-errors"), ignoreError)
			MAP_OPTIONS_STR2INT(_T("max-dir-depth"), max_dir_depth)
			MAP_OPTIONS_BOOL_EX(_T("filter"), finder.bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), finder.bFilterAll, _T("all"), _T("any"))
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
	if (!finder.has_filter()) {
		message = _T("Missing filter argument");
		return NSCAPI::returnUNKNOWN;
	}
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	finder.debug_ = debug;
	finder.now = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	if (debug)
		NSC_DEBUG_MSG_STD(_T("NOW: ") + strEx::format_filetime(finder.now));
	finder.syntax = syntax;
	NSC_error errors;
	for (std::list<std::wstring>::const_iterator pit = paths.begin(); pit != paths.end(); ++pit) {
		recursive_scan<file_filter_function_ex>(*pit, pattern, 0, max_dir_depth, finder, &errors, debug);
		if (!ignoreError && finder.hasError()) {
			message = _T("Error when scanning: ") + (*pit) + _T(" check log for details (") + strEx::itos(finder.error_count) + _T(": ") + finder.last_error + _T(")");
			return NSCAPI::returnUNKNOWN;
		}
	}
	message  = finder.render(masterSyntax);
	if (!alias.empty())
		query.alias = alias;
	else
		query.alias = _T("found files");
	query.runCheck(finder.hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4))) {
		message = message.substr(0, truncate-4) + _T("...");
		//perf = _T("");
	}
	if (message.empty())
		message = _T("CheckFile ok");
	return returnCode;
}
struct file_container : public file_info {
	std::wstring error_;

	static file_container get(std::wstring file, unsigned long long now) {

		BY_HANDLE_FILE_INFORMATION _info;

		HANDLE hFile = CreateFile(file.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			return file_container(file, _T("Could not open file: ") + file);
		}
		GetFileInformationByHandle(hFile, &_info);
		CloseHandle(hFile);
		file_container info(_info, file);
		info.ullNow = now;
		return info;
	}


	file_container(const BY_HANDLE_FILE_INFORMATION info, std::wstring file) : file_info(info, file_helpers::meta::get_path(file), file_helpers::meta::get_filename(file)) {}
	file_container(std::wstring file, std::wstring error) : error_(error), file_info(file_helpers::meta::get_path(file), file_helpers::meta::get_filename(file)) {}

	bool has_errors() {
		return !error_.empty();
	}
	std::wstring get_error() {
		return error_;
	}

};

typedef checkHolders::ExactBounds<checkHolders::NumericBounds<checkHolders::disk_size_type, checkHolders::disk_size_handler<checkHolders::disk_size_type> > > ExactBoundsDiscSize;


typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULong> ExactULongContainer;
typedef checkHolders::CheckContainer<ExactBoundsDiscSize> DiscSizeContainer;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsTime> DateTimeContainer;

struct check_file_size : public checkHolders::check_proxy_container<file_container, DiscSizeContainer> {
	check_file_size() { set_alias(_T("size")); }
	checkHolders::disk_size_type get_value(file_container &value) {
		return value.ullSize;
	}
};
struct check_file_line_count : public checkHolders::check_proxy_container<file_container, ExactULongContainer> {
	check_file_line_count() { set_alias(_T("line-count")); }
	unsigned long get_value(file_container &value) {
		return value.get_line_count();
	}
};
struct check_file_dates : public checkHolders::check_proxy_container<file_container, DateTimeContainer> {
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
	unsigned long long get_value(file_container &value) {
		if (type_ == date_creation)
			return value.ullCreationTime;
		if (type_ == date_access)
			return value.ullLastAccessTime;
		if (type_ == date_written)
			return value.ullLastWriteTime;
		return -1;
	}
};

typedef checkHolders::check_multi_container<file_container> check_file_multi;
struct check_file_factories {
	static checkHolders::check_proxy_interface<file_container>* size() {
		return new check_file_size();
	}
	static checkHolders::check_proxy_interface<file_container>* line_count() {
		return new check_file_line_count();
	}
	static checkHolders::check_proxy_interface<file_container>* access() {
		return new check_file_dates(check_file_dates::date_access);
	}
	static checkHolders::check_proxy_interface<file_container>* creation() {
		return new check_file_dates(check_file_dates::date_creation);
	}
	static checkHolders::check_proxy_interface<file_container>* written() {
		return new check_file_dates(check_file_dates::date_written);
	}
};

#define MAP_FACTORY_PB(value, obj) \
		else if ((p__.first == _T("check")) && (p__.second == ##value)) { checker.add_check(check_file_factories::obj()); }


NSCAPI::nagiosReturn CheckDisk::CheckSingleFile(const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	check_file_multi checker;
	typedef std::pair<int,file_filter> filteritem_type;
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
	unsigned __int64 nowi64 = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	//finder.syntax = syntax;
	for (std::list<std::wstring>::const_iterator pit = files.begin(); pit != files.end(); ++pit) {
		file_container info = file_container::get(*pit, nowi64);
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
NSCAPI::nagiosReturn CheckDisk::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &msg, std::wstring &perf) {
	if (command == _T("CheckFileSize")) {
		return CheckFileSize(argLen, char_args, msg, perf);
	} else if (command == _T("CheckDriveSize")) {
		return CheckDriveSize(argLen, char_args, msg, perf);
	} else if (command == _T("CheckFile")) {
		return CheckFile(argLen, char_args, msg, perf);
	} else if (command == _T("CheckFile2")) {
		return CheckFile2(argLen, char_args, msg, perf);
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
