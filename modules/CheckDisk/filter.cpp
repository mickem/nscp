/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include <simple_timer.hpp>
#include <str/xtos.hpp>
#include "filter.hpp"

#include "file_finder.hpp"

using namespace boost::assign;
using namespace parsers::where;

const int file_type_file = 1;
const int file_type_dir = 2;
const int file_type_error = -1;
//////////////////////////////////////////////////////////////////////////

int convert_new_type(parsers::where::evaluation_context context, std::string str) {
	if (str == "critical")
		return 1;
	if (str == "error")
		return 2;
	if (str == "warning" || str == "warn")
		return 3;
	if (str == "informational" || str == "info" || str == "information" || str == "success" || str == "auditSuccess")
		return 4;
	if (str == "debug" || str == "verbose")
		return 5;
	try {
		return str::stox<int>(str);
	} catch (const std::exception&) {
		context->error("Failed to convert: " + str);
		return 2;
	}
}


parsers::where::node_type fun_convert_type(boost::shared_ptr<file_filter::filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	try {
		std::string key = subject->get_string_value(context);
		if (key == "file")
			return parsers::where::factory::create_int(file_type_file);
		if (key == "dir")
			return parsers::where::factory::create_int(file_type_dir);
		context->error("Failed to convert: " + key + " not file or dir");
		return parsers::where::factory::create_int(file_type_error);
	} catch (const std::exception &e) {
		context->error("Failed to convert type expression: " + utf8::utf8_from_native(e.what()));
	}
	return parsers::where::factory::create_int(-1);
}

file_filter::filter_obj_handler::filter_obj_handler() {

	const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

	registry_.add_string()
		("path", &filter_obj::get_path, "Path of file")
		("version", boost::bind(&filter_obj::get_version, _1), "Windows exe/dll file version")
		("filename", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		("file", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		("name", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		("access_l", boost::bind(&filter_obj::get_access_sl, _1), "Last access time (local time)")
		("creation_l", boost::bind(&filter_obj::get_creation_sl, _1), "When file was created (local time)")
		("written_l", boost::bind(&filter_obj::get_written_sl, _1), "When file was last written  to (local time)")
		("access_u", boost::bind(&filter_obj::get_access_su, _1), "Last access time (UTC)")
		("creation_u", boost::bind(&filter_obj::get_creation_su, _1), "When file was created (UTC)")
		("written_u", boost::bind(&filter_obj::get_written_su, _1), "When file was last written  to (UTC)")
		;

	registry_.add_int()
		("size", type_size, boost::bind(&filter_obj::get_size, _1), "File size").add_scaled_byte(std::string(""), " size")
		("line_count", boost::bind(&filter_obj::get_line_count, _1), "Number of lines in the file (text files)")
		("access", type_date, boost::bind(&filter_obj::get_access, _1), "Last access time")
		("creation", type_date, boost::bind(&filter_obj::get_creation, _1), "When file was created")
		("written", type_date, boost::bind(&filter_obj::get_write, _1), "When file was last written to")
		("write", type_date, boost::bind(&filter_obj::get_write, _1), "Alias for written")
		("age", type_int, boost::bind(&filter_obj::get_age, _1), "Seconds since file was last written")
		("type", type_custom_type, boost::bind(&filter_obj::get_type, _1), "Type of item (file or dir)")
		("total", type_bool, boost::bind(&filter_obj::is_total, _1),
			"True if this is the total object").no_perf();
	;

	registry_.add_converter()
		(type_custom_type, &fun_convert_type)
		;


	registry_.add_human_string()
		("access", boost::bind(&filter_obj::get_access_su, _1), "")
		("creation", boost::bind(&filter_obj::get_creation_su, _1), "")
		("written", boost::bind(&filter_obj::get_written_su, _1), "")
		("type", boost::bind(&filter_obj::get_type_su, _1), "")
		;
}

//////////////////////////////////////////////////////////////////////////

#ifdef WIN32
boost::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::get(unsigned long long now, const WIN32_FIND_DATA info, boost::filesystem::path path) {
	return boost::shared_ptr<file_filter::filter_obj>(new file_filter::filter_obj(path, utf8::cvt<std::string>(info.cFileName), now,
		(info.ftCreationTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.ftCreationTime.dwLowDateTime,
		(info.ftLastAccessTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.ftLastAccessTime.dwLowDateTime,
		(info.ftLastWriteTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.ftLastWriteTime.dwLowDateTime,
		(info.nFileSizeHigh * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)info.nFileSizeLow,
		info.dwFileAttributes
		));
};
#endif
boost::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::get_total(unsigned long long now) {
	return boost::shared_ptr<file_filter::filter_obj>(new file_filter::filter_obj("", "total", now, now, now, now, 0));
}

std::string file_filter::filter_obj::get_version() {
	if (cached_version)
		return *cached_version;
	std::string fullpath = (path / filename).string();

	DWORD dwDummy;
	DWORD dwFVISize = GetFileVersionInfoSize(utf8::cvt<std::wstring>(fullpath).c_str(), &dwDummy);
	if (dwFVISize == 0)
		return "";
	LPBYTE lpVersionInfo = new BYTE[dwFVISize + 1];
	if (!GetFileVersionInfo(utf8::cvt<std::wstring>(fullpath).c_str(), 0, dwFVISize, lpVersionInfo)) {
		delete[] lpVersionInfo;
		//handler->error("Failed to get version for " + fullpath + ": " + error::lookup::last_error());
		return "";
	}
	UINT uLen;
	VS_FIXEDFILEINFO *lpFfi;
	if (!VerQueryValue(lpVersionInfo, L"\\", (LPVOID *)&lpFfi, &uLen)) {
		delete[] lpVersionInfo;
		//handler->error("Failed to query version for " + fullpath + ": " + error::lookup::last_error());
		return "";
	}
	DWORD dwFileVersionMS = lpFfi->dwFileVersionMS;
	DWORD dwFileVersionLS = lpFfi->dwFileVersionLS;
	delete[] lpVersionInfo;
	DWORD dwLeftMost = HIWORD(dwFileVersionMS);
	DWORD dwSecondLeft = LOWORD(dwFileVersionMS);
	DWORD dwSecondRight = HIWORD(dwFileVersionLS);
	DWORD dwRightMost = LOWORD(dwFileVersionLS);
	cached_version.reset(str::xtos(dwLeftMost) + "." +
		str::xtos(dwSecondLeft) + "." +
		str::xtos(dwSecondRight) + "." +
		str::xtos(dwRightMost));
	return *cached_version;
}

unsigned long long file_filter::filter_obj::get_type() {
	return file_finder::is_directory(attributes)? file_type_dir: file_type_file;
}

std::string file_filter::filter_obj::get_type_su() {
	return file_finder::is_directory(attributes) ? "dir" : "file";
}

unsigned long file_filter::filter_obj::get_line_count() {
	if (cached_count)
		return *cached_count;

	unsigned long count = 0;
	std::string fullpath = (path / filename).string();
	FILE * pFile = fopen(fullpath.c_str(), "r");;
	if (pFile == NULL)
		return 0;
	int c;
	do {
		c = fgetc(pFile);
		if (c == '\r') {
			c = fgetc(pFile);
			count++;
		} else if (c == '\n') {
			c = fgetc(pFile);
			count++;
		}
	} while (c != EOF);
	fclose(pFile);
	cached_count.reset(count);
	return *cached_count;
}

void file_filter::filter_obj::add(boost::shared_ptr<file_filter::filter_obj> info) {
	ullSize += info->ullSize;
}

//////////////////////////////////////////////////////////////////////////