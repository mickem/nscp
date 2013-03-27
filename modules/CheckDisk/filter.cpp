#include "stdafx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>
#include <parsers/filter/where_filter.hpp>
#include <parsers/filter/where_filter_impl.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"

#include "file_info.hpp"
#include "file_finder.hpp"

//#include <config.h>

using namespace boost::assign;
using namespace parsers::where;

file_filter::filter_obj_handler::filter_obj_handler() {
		insert(types)
			("filename", (type_string))
			("name", (type_string))
			("version", (type_string))
			("path", (type_string))
			("line_count", (type_int))
			("size", (type_size))
			("access", (type_date))
			("creation", (type_date))
			("written", (type_date));
	}

bool file_filter::filter_obj_handler::has_variable(std::string key) {
	return types.find(key) != types.end();
}
parsers::where::value_type file_filter::filter_obj_handler::get_type(std::string key) {
	types_type::const_iterator cit = types.find(key);
	if (cit == types.end())
		return parsers::where::type_invalid;
	return cit->second;
}
bool file_filter::filter_obj_handler::can_convert(parsers::where::value_type from, parsers::where::value_type to) {
	if ((from == parsers::where::type_string)&&(to == type_custom_severity))
		return true;
	if ((from == parsers::where::type_string)&&(to == type_custom_type))
		return true;
	return false;
}

file_filter::filter_obj_handler::base_handler::bound_string_type file_filter::filter_obj_handler::bind_simple_string(std::string key) {
	base_handler::bound_string_type ret;
	if (key == "filename")
		ret = &filter_obj::get_filename;
	else if (key == "name")
		ret = &filter_obj::get_filename;
	else if (key == "path")
		ret = &filter_obj::get_path;
	else if (key == "version")
		ret = boost::bind(&filter_obj::get_version, _1, this);
	else
		NSC_DEBUG_MSG_STD("Failed to bind (string): " + key);
	return ret;
}



file_filter::filter_obj_handler::base_handler::bound_int_type file_filter::filter_obj_handler::bind_simple_int(std::string key) {
	base_handler::bound_int_type ret;
	if (key == "size")
		ret = &filter_obj::get_size;
	else if (key == "line_count")
		ret = &filter_obj::get_line_count;
	else if (key == "access")
		ret = &filter_obj::get_access;
	else if (key == "creation")
		ret = &filter_obj::get_creation;
	else if (key == "written")
		ret = &filter_obj::get_write;
	else
		NSC_DEBUG_MSG_STD("Failed to bind (int): " + key);
	return ret;
}

bool file_filter::filter_obj_handler::has_function(parsers::where::value_type to, std::string name, ast_expr_type *subject) {
	return false;
}
file_filter::filter_obj_handler::base_handler::bound_function_type file_filter::filter_obj_handler::bind_simple_function(parsers::where::value_type to, std::string name, ast_expr_type *subject) {
	base_handler::bound_function_type ret;
	return ret;
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



std::string file_filter::filter_obj::get_version(filter_obj_handler *handler) {
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
		handler->error("Failed to get version for " + fullpath + ": " + error::lookup::last_error());
		return "";
	}
	UINT uLen;
	VS_FIXEDFILEINFO *lpFfi;
	if (!VerQueryValue( lpVersionInfo , _T("\\") , (LPVOID *)&lpFfi , &uLen )) {
		delete [] lpVersionInfo;
		handler->error("Failed to query version for " + fullpath + ": " + error::lookup::last_error());
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
	cached_count.reset(count);
	return *cached_count;
}


std::string file_filter::filter_obj::render(std::string syntax, std::string datesyntax) {
	strEx::replace(syntax, "%path%", path.string());
	strEx::replace(syntax, "%filename%", filename);
	strEx::replace(syntax, "%creation%", format::format_filetime(ullCreationTime, datesyntax));
	strEx::replace(syntax, "%access%", format::format_filetime(ullLastAccessTime, datesyntax));
	strEx::replace(syntax, "%write%", format::format_filetime(ullLastWriteTime, datesyntax));
	strEx::replace(syntax, "%creation-raw%", strEx::s::xtos(ullCreationTime));
	strEx::replace(syntax, "%access-raw%", strEx::s::xtos(ullLastAccessTime));
	strEx::replace(syntax, "%write-raw%", strEx::s::xtos(ullLastWriteTime));
	strEx::replace(syntax, "%now-raw%", strEx::s::xtos(ullNow));
/*
	strEx::replace(syntax, "%creation-d%", strEx::format_filetime(ullCreationTime, DATE_FORMAT));
	strEx::replace(syntax, "%access-d%", strEx::format_filetime(ullLastAccessTime, DATE_FORMAT));
	strEx::replace(syntax, "%write-d%", strEx::format_filetime(ullLastWriteTime, DATE_FORMAT));
*/
	strEx::replace(syntax, "%size%", format::format_byte_units(ullSize));
	if (cached_version)
		strEx::replace(syntax, "%version%", *cached_version);
	if (cached_count)
		strEx::replace(syntax, "%line_count%", strEx::s::xtos(*cached_count));
	return syntax;
}

//////////////////////////////////////////////////////////////////////////

struct where_mode_filter : public file_filter::filter_engine_type {
	typedef file_filter::filter_obj_handler handler_type;
	typedef file_filter::filter_obj object_type;

	typedef boost::shared_ptr<handler_type> handler_instance_type;

	file_filter::filter_argument data;
	parsers::where::parser ast_parser;
	handler_instance_type object_handler;

	where_mode_filter(file_filter::filter_argument data) : data(data), object_handler(handler_instance_type(new handler_type())) {}
	bool boot() { return true; }

	bool validate(std::string &message) {
		if (data->debug)
			data->error->report_debug("Parsing: " + data->filter);

		if (!ast_parser.parse(data->filter)) {
			data->error->report_error("Parsing failed of '" + data->filter + "' at: " + ast_parser.rest);
		message = "Parsing failed: " + ast_parser.rest;
		return false;
		}
		if (data->debug)
			data->error->report_debug("Parsing succeeded: " + ast_parser.result_as_tree());

		if (!ast_parser.derive_types(object_handler) || object_handler->has_error()) {
			message = "Invalid types: " + object_handler->get_error();
			return false;
		}
			if (data->debug)
				data->error->report_debug("Type resolution succeeded: " + ast_parser.result_as_tree());

		if (!ast_parser.bind(object_handler) || object_handler->has_error()) {
			message = "Variable and function binding failed: " + object_handler->get_error();
			return false;
		}
			if (data->debug)
				data->error->report_debug("Binding succeeded: " + ast_parser.result_as_tree());

		if (!ast_parser.static_eval(object_handler) || object_handler->has_error()) {
			message = "Static evaluation failed: " + object_handler->get_error();
			return false;
		}
			if (data->debug)
				data->error->report_debug("Static evaluation succeeded: " + ast_parser.result_as_tree());

		return true;
	}

	bool match(boost::shared_ptr<object_type> record) {
		object_handler->set_current_element(record);
		bool ret = ast_parser.evaluate(object_handler);
		if (object_handler->has_error()) {
			data->error->report_error("Error: " + object_handler->get_error());
		}
		return ret;
	}

	std::string get_name() {
		return "where";
	}
	std::string get_subject() { return data->filter; }
};

//////////////////////////////////////////////////////////////////////////

file_filter::file_finder_data_arguments::file_finder_data_arguments(std::string pattern, int max_depth, file_filter::filter_argument_type::error_type error, std::string syntax, std::string datesyntax, bool debug) 
		: debugThreshold(0), bFilterIn(true), bFilterAll(false), bShowDescriptions(false), max_level(max_depth), pattern(pattern) 
		, file_filter::file_finder_data_arguments::parent_type(error, syntax, datesyntax, debug)
{
	FILETIME ft_now;
	GetSystemTimeAsFileTime(&ft_now);
	now = ((ft_now.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)ft_now.dwLowDateTime);
}

struct size_file_engine : public file_filter::filesize_engine_interface_type {
	typedef file_filter::filter_obj_handler handler_type;
	typedef file_filter::filter_obj object_type;

	typedef boost::shared_ptr<handler_type> handler_instance_type;

	size_file_engine() : size(0) {}

	bool boot() { return true; }
	bool validate(std::string &message) {
		return true;
	}
	bool match(boost::shared_ptr<object_type> record) {
		if (!file_helpers::checks::is_directory(record->attributes)) {
			size += record->get_size();
		}
		return true;
	}
	std::string get_name() { return "file-size"; }
	std::string get_subject() { return "TODO"; }
	unsigned long long get_size() {
		return size;
	}
private:  
	unsigned long long size;

};



//////////////////////////////////////////////////////////////////////////
file_filter::filter_engine file_filter::factories::create_engine(file_filter::filter_argument arg) {
	return filter_engine(new where_mode_filter(arg));
}
file_filter::filesize_engine_interface file_filter::factories::create_size_engine() {
	return filesize_engine_interface(new size_file_engine());
}
file_filter::filter_argument file_filter::factories::create_argument(std::string pattern, int max_depth, std::string syntax, std::string datesyntax) {
	return filter_argument(new file_filter::file_finder_data_arguments(pattern, max_depth, file_filter::filter_argument_type::error_type(new where_filter::nsc_error_handler(GET_CORE())), syntax, datesyntax));
}

file_filter::filter_result file_filter::factories::create_result(file_filter::filter_argument arg) {
	return filter_result(new where_filter::simple_count_result<filter_obj>(arg));
}





