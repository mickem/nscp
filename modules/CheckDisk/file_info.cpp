#include "stdafx.h"
#include "file_info.hpp"
#include <strEx.h>
#include <error.hpp>
#include <config.h>

#include "filter.hpp"

// 
// file_info file_info::get_2(__int64 now, std::wstring path, std::wstring file) {
// 	WIN32_FILE_ATTRIBUTE_DATA data;
// 	if (!GetFileAttributesEx((path + _T("\\") + file).c_str(), GetFileExInfoStandard, reinterpret_cast<LPVOID>(&data))) {
// 		file_info ret;
// 		ret.error = _T("Could not open file (2) ") + path + _T("\\") + file + _T(": ") + error::lookup::last_error();
// 		return ret;
// 	}
// 	return file_info(now, data, path, file);
// }
// file_info file_info::get_1(__int64 now, std::wstring path, std::wstring file) {
// 	HANDLE hFile = CreateFile((path + _T("\\") + file).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
// 	if (hFile == INVALID_HANDLE_VALUE) {
// 		file_info ret;
// 		ret.error = _T("Could not open file (1) ") + path + _T("\\") + file + _T(": ") + error::lookup::last_error();
// 		return ret;
// 	}
// 	BY_HANDLE_FILE_INFORMATION _info;
// 	GetFileInformationByHandle(hFile, &_info);
// 	CloseHandle(hFile);
// 	return file_info(now, _info, path, file);
// }
// 
// 
// 
// file_container file_container::get(std::wstring file) {
// 	FILETIME now;
// 	GetSystemTimeAsFileTime(&now);
// 	unsigned __int64 nowi64 = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
// 	return get(file, nowi64);
// }
// 
// file_container file_container::get(std::wstring file, unsigned long long now) {
// 
// 	BY_HANDLE_FILE_INFORMATION _info;
// 
// 	HANDLE hFile = CreateFile(file.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
// 		0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
// 	if (hFile == INVALID_HANDLE_VALUE) {
// 		return file_container(now, file, _T("Could not open file: ") + file);
// 	}
// 	GetFileInformationByHandle(hFile, &_info);
// 	CloseHandle(hFile);
// 	file_container info(now, _info, file);
// 	//info.ullNow = now;
// 	return info;
// }


bool file_finder::filter::matchFilter(file_filter::filter_obj *value) const {
	if ((size.hasFilter())&&(size.matchFilter(value->get_size())))
		return true;
	else if (creation.hasFilter()&&creation.matchFilter(value->get_creation()))
		return true;
	else if (accessed.hasFilter()&&accessed.matchFilter(value->get_access()))
		return true;
	else if (written.hasFilter()&&written.matchFilter(value->get_write()))
		return true;
	else if ((version.hasFilter())&&(version.matchFilter(value->get_version())))
		return true;
	else if ((line_count.hasFilter())&&(line_count.matchFilter(value->get_line_count())))
		return true;
	return false;
}
