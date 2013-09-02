#include "stdafx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"

#include "file_info.hpp"
#include "file_finder.hpp"

using namespace boost::assign;
using namespace parsers::where;


//////////////////////////////////////////////////////////////////////////


file_filter::filter_obj_handler::filter_obj_handler() {

	registry_.add_string()
		("path", &filter_obj::get_path, "TODO")
		("version", boost::bind(&filter_obj::get_version, _1), "TODO")
		("filename", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		("file", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		("name", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		;

	registry_.add_int()
		("size", boost::bind(&filter_obj::get_size, _1), "File size")
		("line_count", boost::bind(&filter_obj::get_line_count, _1), "Number of lines matching filter")
		("access", type_date, boost::bind(&filter_obj::get_access, _1), "Number of lines matching filter")
		("creation", type_date, boost::bind(&filter_obj::get_creation, _1), "Number of lines matching filter")
		("written", type_date, boost::bind(&filter_obj::get_write, _1), "Number of lines matching filter")
		;

}

//////////////////////////////////////////////////////////////////////////

#ifdef WIN32
file_filter::filter_obj file_filter::filter_obj::get(unsigned long long now, const WIN32_FILE_ATTRIBUTE_DATA info, boost::filesystem::path path, std::string filename) {
	return file_filter::filter_obj(path, utf8::cvt<std::string>(filename), now, 
		(info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime,
		(info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime,
		(info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime,
		(info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow
		);
};
file_filter::filter_obj file_filter::filter_obj::get(unsigned long long now, const BY_HANDLE_FILE_INFORMATION info, boost::filesystem::path path, std::wstring filename) {
	return file_filter::filter_obj(path, utf8::cvt<std::string>(filename), now, 
		(info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime,
		(info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime,
		(info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime,
		(info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow
	);
};
boost::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::get(unsigned long long now, const WIN32_FIND_DATA info, boost::filesystem::path path) {
	return boost::shared_ptr<file_filter::filter_obj>(new file_filter::filter_obj(path, utf8::cvt<std::string>(info.cFileName), now, 
		(info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime, 
		(info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime,
		(info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime,
		(info.nFileSizeHigh * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)info.nFileSizeLow
		));
	//attributes = info.dwFileAttributes;
};
#endif
file_filter::filter_obj file_filter::filter_obj::get(unsigned long long now, boost::filesystem::path path, std::string filename) {
 	WIN32_FILE_ATTRIBUTE_DATA data;
 	if (!GetFileAttributesEx(utf8::cvt<std::wstring>(path.string() + "\\" + filename).c_str(), GetFileExInfoStandard, reinterpret_cast<LPVOID>(&data))) {
		throw new file_object_exception("Could not open file (2) " + path.string() + "\\" + filename + ": " + error::lookup::last_error());
	}
	return get(now, data, path, filename);
}
file_filter::filter_obj file_filter::filter_obj::get(std::string file) {
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	unsigned __int64 nowi64 = ((now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)now.dwLowDateTime);
	return get(nowi64, file);
}
file_filter::filter_obj file_filter::filter_obj::get(unsigned long long now, std::string file) {

	BY_HANDLE_FILE_INFORMATION _info;

	HANDLE hFile = CreateFile(utf8::cvt<std::wstring>(file).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (hFile == INVALID_HANDLE_VALUE) {
		throw new file_object_exception("Could not open file (2) " + utf8::cvt<std::string>(file) + ": " + utf8::cvt<std::string>(error::lookup::last_error()));
	}
	GetFileInformationByHandle(hFile, &_info);
	CloseHandle(hFile);
	return get(now, _info, file_helpers::meta::get_path(file), file_helpers::meta::get_filename(file));
}



std::string file_filter::filter_obj::get_version() {
	if (cached_version)
		return *cached_version;
	std::string fullpath = (path / filename).string();

	DWORD dwDummy;
	DWORD dwFVISize = GetFileVersionInfoSize(utf8::cvt<std::wstring>(fullpath).c_str(), &dwDummy);
	if (dwFVISize == 0)
		return "";
	LPBYTE lpVersionInfo = new BYTE[dwFVISize+1];
	if (!GetFileVersionInfo(utf8::cvt<std::wstring>(fullpath).c_str(),0,dwFVISize,lpVersionInfo)) {
		delete [] lpVersionInfo;
		//handler->error("Failed to get version for " + fullpath + ": " + error::lookup::last_error());
		return "";
	}
	UINT uLen;
	VS_FIXEDFILEINFO *lpFfi;
	if (!VerQueryValue( lpVersionInfo , _T("\\") , (LPVOID *)&lpFfi , &uLen )) {
		delete [] lpVersionInfo;
		//handler->error("Failed to query version for " + fullpath + ": " + error::lookup::last_error());
		return "";
	}
	DWORD dwFileVersionMS = lpFfi->dwFileVersionMS;
	DWORD dwFileVersionLS = lpFfi->dwFileVersionLS;
	delete [] lpVersionInfo;
	DWORD dwLeftMost = HIWORD(dwFileVersionMS);
	DWORD dwSecondLeft = LOWORD(dwFileVersionMS);
	DWORD dwSecondRight = HIWORD(dwFileVersionLS);
	DWORD dwRightMost = LOWORD(dwFileVersionLS);
	cached_version.reset(strEx::s::xtos(dwLeftMost) + "." +
		strEx::s::xtos(dwSecondLeft) + "." +
		strEx::s::xtos(dwSecondRight) + "." +
		strEx::s::xtos(dwRightMost));
	return *cached_version;
}

unsigned long file_filter::filter_obj::get_line_count() {
	if (cached_count)
		return *cached_count;

	unsigned long count = 0;
	std::string fullpath = (path / filename).string();
	FILE * pFile = fopen(fullpath.c_str(),"r");;
	if (pFile==NULL) 
		return 0;
	int c;
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
	cached_count.reset(count);
	return *cached_count;
}

//////////////////////////////////////////////////////////////////////////
