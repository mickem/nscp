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

CheckDisk::CheckDisk() : show_errors_(false) {
}
CheckDisk::~CheckDisk() {
}

bool is_directory(DWORD dwAttr) {
	return ((dwAttr != INVALID_FILE_ATTRIBUTES) && ((dwAttr&FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
}

bool CheckDisk::loadModule(NSCAPI::moduleLoadMode mode) {

	try {
		GET_CORE()->registerCommand(_T("check_drive"), _T("Check the disk space usage of a drive or volume."));
		GET_CORE()->registerCommand(_T("check_file"), _T("Check a single file and/or folder."));
		GET_CORE()->registerCommand(_T("check_files"), _T("Check a set of files and/or directories"));

		show_errors_ = SETTINGS_GET_BOOL(check_disk::SHOW_ERRORS);
	} catch (nscapi::nscapi_exception &e) {
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

using namespace boost::filesystem;

struct find_options {
	bool recursive;
	int max_depth;
	int current_depth;
	bool single_file;

	find_options() : current_depth(0), single_file(false) {}

	inline find_options go_down() { 
		find_options op = *this;
		op.current_depth++;
		return op;
	}
	inline bool has_all(std::list<file_item> & files) {
		return single_file;
	}
};
struct file_item {
	bool is_directory() {
		return true;
	}
	bool is_file() {
		return false;
	}
	std::wstring get_name() {
		return _T("hello");
	}
};

struct file_filter {
	virtual bool matches(file_item &) = 0;
};


bool find_files(const boost::filesystem::wpath & path, const find_options options, file_filter filter, std::list<file_item> & files)
{
	if ( !exists( path ) ) return false;
	directory_iterator end_itr; // default construction yields past-the-end
	for ( directory_iterator itr(path); itr != end_itr; ++itr ) {
		if ( is_directory(itr->status()) ) {
			if (find_files(itr->path(), options.go_down(), filter, files) && options.has_all(files))
				return true;
		} else {
			file_item file(itr->leaf());
			if (filter.matches(file)) {
				files.push_back(file);
				if (options.has_all(files))
					return true;
			}
		}
	}
	return false;
}

void CheckDisk::check_file(PluginCommand::Request *request, PluginCommand::Response *response) {
	const boost::filesystem::wpath & path;
	const find_options options(...);
	file_filter filter(...);
	std::list<file_item> files;



	find_files(path, options, filter, files);
}


void CheckDisk::handleCommand(std::wstring command, PluginCommand::Request *request, PluginCommand::Response *response) {
	if (command == _T("check_file"))
		check_file(request, response);

}
/*
NSCAPI::nagiosReturn CheckDisk::handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &msg, std::wstring &perf) {
	if (command == _T("check_drive")) {
		//return CheckFileSize(argLen, char_args, msg, perf);
	}	
	return NSCAPI::returnIgnored;
}
*/

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gCheckDisk);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckDisk);
