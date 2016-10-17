/*
* Copyright 2009, Google Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//
// Wrapper class for using the Breakpad crash reporting system.
// (adapted from code in Google Gears)
//
#ifdef USE_BREAKPAD

#include <windows.h>
#include <assert.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>
#include <time.h>
#include <string>
#include <iostream>
#include <file_helpers.hpp>
#include <breakpad/breakpad_config.hpp>
#include <breakpad/exception_handler_win32.hpp>
#include <client/windows/handler/exception_handler.h>
#ifdef WIN32
#include <ServiceCmd.h>
#endif
#include <boost/foreach.hpp>

// Some simple string typedefs
#define STRING16(x) reinterpret_cast<const char16*>(x)

typedef wchar_t char16;

namespace std {
	typedef wstring string16;
}

ExceptionManager* ExceptionManager::instance_ = NULL;

ExceptionManager::ExceptionManager(bool catch_entire_process)
	: catch_entire_process_(catch_entire_process),
	exception_handler_(NULL) {
	assert(!instance_);
	instance_ = this;
}

ExceptionManager::~ExceptionManager() {
	if (exception_handler_)
		delete exception_handler_;
	assert(instance_ == this);
	instance_ = NULL;
}

static HMODULE GetModuleHandleFromAddress(void *address) {
	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery(address, &mbi, sizeof(mbi));
	return static_cast<HMODULE>(mbi.AllocationBase);
}

// Gets the handle to the currently executing module.
static HMODULE GetCurrentModuleHandle() {
	// pass a pointer to the current function
	return GetModuleHandleFromAddress(GetCurrentModuleHandle);
}

// static bool IsAddressInCurrentModule(void *address) {
// 	return GetCurrentModuleHandle() == GetModuleHandleFromAddress(address);
// }

// Called back when an exception occurs - we can decide here if we
// want to handle this crash...
//
static bool FilterCallback(void *context, EXCEPTION_POINTERS *exinfo, MDRawAssertionInfo*) {
	// g_logger will be NULL if user opts out of metrics/crash reporting
	//if (!g_logger) return false;

	ExceptionManager* this_ptr = reinterpret_cast<ExceptionManager*>(context);

	if (this_ptr->catch_entire_process())
		return true;

	if (!exinfo)
		return true;

	return TRUE;
	//return IsAddressInCurrentModule(exinfo->ExceptionRecord->ExceptionAddress);
}

std::string modulePath() {
	unsigned int buf_len = 4096;
	TCHAR* buffer = new TCHAR[buf_len + 1];
	GetModuleFileName(NULL, buffer, buf_len);
	std::string path = utf8::cvt<std::string>(buffer);
	delete[] buffer;
	std::string::size_type pos = path.rfind('\\');
	return path.substr(0, pos);
}

void report_error(std::string err) {
	std::cout << "ERR: " << err << std::endl;
}
void report_info(std::string err) {
	std::cout << "INF: " << err << std::endl;
}

std::string build_commandline(std::vector<std::string> &commands) {
	std::string command_line;
	BOOST_FOREACH(const std::string &s, commands) {
		if (!command_line.empty())
			command_line += " ";
		command_line += "\"" + s + "\"";
	}
	return command_line;
}

void run_proc(std::string command_line) {
	report_info("Running: " + command_line);
	// execute the process
	STARTUPINFO startup_info = { 0 };
	startup_info.cb = sizeof(startup_info);
	PROCESS_INFORMATION process_info = { 0 };
	CreateProcessW(NULL, const_cast<char16 *>(utf8::cvt<std::wstring>(command_line).c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &startup_info, &process_info);
	CloseHandle(process_info.hProcess);
	CloseHandle(process_info.hThread);
}

void run_command(ExceptionManager* this_ptr, std::string exe, std::string command, std::string minidump, std::string target) {
	std::vector<std::string> commands;
	commands.push_back(exe);
	commands.push_back(command);
	commands.push_back(minidump);
	commands.push_back(this_ptr->application());
	commands.push_back(this_ptr->version());
	commands.push_back(this_ptr->date());
	commands.push_back(target);
	run_proc(build_commandline(commands));

	Sleep(500);
}

static bool MinidumpCallback(const wchar_t *minidump_folder, const wchar_t *minidump_id, void *context, EXCEPTION_POINTERS*, MDRawAssertionInfo*, bool) {
	ExceptionManager* this_ptr = reinterpret_cast<ExceptionManager*>(context);
	report_info("Detected crash...");

	std::string minidump_path = utf8::cvt<std::string>(minidump_folder) + "\\" + utf8::cvt<std::string>(minidump_id) + ".dmp";
	if (minidump_path.length() >= MAX_PATH) {
		report_error("Path to long");
		return false;
	}
	if (!boost::filesystem::is_regular(minidump_path)) {
		report_error("Failed to create mini dump please check that you have a proper version of dbghlp.dll");
		return false;
	}

	std::string path = modulePath() + "\\reporter.exe";
	if (path.length() >= MAX_PATH) {
		report_error("Path to long");
		return false;
	}

	if (!boost::filesystem::is_regular(path)) {
		report_error("Failed to find reporter.exe");
		return false;
	}
	if (this_ptr->is_archive()) {
		run_command(this_ptr, path, "archive", minidump_path, this_ptr->target());
	}
	if (this_ptr->is_send()) {
		if (this_ptr->is_send_ui())
			run_command(this_ptr, path, "send-gui", minidump_path, this_ptr->target());
		else
			run_command(this_ptr, path, "send", minidump_path, this_ptr->target());
	}

#ifdef WIN32
	if (this_ptr->is_restart()) {
		std::vector<std::string> commands;
		try {
			if (!serviceControll::isStarted(utf8::cvt<std::wstring>(this_ptr->service()))) {
				report_error("Service not started, not restarting...");
				return true;
			}
		} catch (...) {
			report_error("Failed to check service state");
		}
		commands.push_back(path);
		commands.push_back("restart");
		commands.push_back(this_ptr->service());
		run_proc(build_commandline(commands));
	}
#endif
	return true;
}

void ExceptionManager::setup_restart(std::string service) {
	service_ = service;
}

void ExceptionManager::setup_submit(bool ui, std::string url) {
	ui_ = ui;
	url_ = url;
}

void ExceptionManager::setup_app(std::string application, std::string version, std::string date) {
	application_ = application;
	version_ = version;
	date_ = date;
}

void ExceptionManager::setup_archive(std::string target) {
	target_ = target;
}

void ExceptionManager::StartMonitoring() {
	if (exception_handler_) { return; }  // don't init more than once
	wchar_t temp_path[MAX_PATH];
	if (!GetTempPathW(MAX_PATH, temp_path)) { return; }
	exception_handler_ = new google_breakpad::ExceptionHandler(temp_path, FilterCallback, MinidumpCallback, this, true);
}
#endif