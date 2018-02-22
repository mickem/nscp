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

#include "eventlog_wrapper.hpp"

#include <nsclient/nsclient_exception.hpp>
#include <str/utils.hpp>

#include "simple_registry.hpp"
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

std::string eventlog_wrapper::find_eventlog_name(const std::string name) {
	try {
		simple_registry::registry_key key(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog");
		BOOST_FOREACH(const std::wstring k, key.get_keys()) {
			try {
				simple_registry::registry_key sub_key(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\" + k);
				std::wstring file = sub_key.get_string(L"DisplayNameFile");
				int id = sub_key.get_int(L"DisplayNameID");
				std::string real_name = error::format::message::from_module(utf8::cvt<std::string>(file), id);
				str::utils::replace(real_name, "\n", "");
				str::utils::replace(real_name, "\r", "");
				if (real_name == name)
					return utf8::cvt<std::string>(k);
			} catch (simple_registry::registry_exception &e) { e; }
		}
		return name;
	} catch (simple_registry::registry_exception &e) {
		return name;
	} catch (...) {
		return name;
	}
}

//////////////////////////////////////////////////////////////////////////
// NEW API impl


eventlog_wrapper_new::eventlog_wrapper_new(const std::string &s_name) {
	name = eventlog_wrapper::find_eventlog_name(s_name);
	open();
}
eventlog_wrapper_new::~eventlog_wrapper_new() {
	if (isOpen())
		close();
}

void eventlog_wrapper_new::open() {
	hContext = eventlog::EvtCreateRenderContext(0, NULL, eventlog::api::EvtRenderContextSystem);
	if (!hContext)
		throw nsclient::nsclient_exception("EvtCreateRenderContext failed: " + error::lookup::last_error());
}
	
void eventlog_wrapper_new::reopen() {
	if (hContext)
		close();
	open();
}

void eventlog_wrapper_new::close() {
	hContext.close();
}


bool eventlog_wrapper_new::notify(HANDLE &handle) {
	handle = CreateEvent(NULL, TRUE, TRUE, NULL);
	if (handle == NULL) {
		NSC_LOG_ERROR_STD("Failed to create event: " + utf8::cvt<std::string>(error::lookup::last_error()));
		return false;
	}

	hLog = eventlog::EvtSubscribe(NULL, handle, utf8::cvt<std::wstring>(name).c_str(), NULL, NULL, NULL, NULL, eventlog::api::EvtSubscribeToFutureEvents);
	if (!hLog) {
		NSC_LOG_ERROR_STD("Failed to subscribe: " + utf8::cvt<std::string>(error::lookup::last_error()));
		return false;
	}
	return true;
}

bool eventlog_wrapper_new::un_notify(HANDLE &handle) {
	hLog.close();
	if (!CloseHandle(handle)) {
		return false;
	}
	return true;
}

void eventlog_wrapper_new::reset_event(HANDLE &handle) {
	ResetEvent(handle);
}


eventlog_filter::filter::object_type eventlog_wrapper_new::read_record(HANDLE &handle) {

	__time64_t ltime;
	_time64(&ltime);

	eventlog::api::EVT_HANDLE hEvents[1];
	DWORD dwReturned = 0;
	if (!eventlog::EvtNext(hLog, 1, hEvents, 100, 0, &dwReturned)) {
		DWORD status = GetLastError();
		if (status == ERROR_NO_MORE_ITEMS || status == ERROR_TIMEOUT)
			return eventlog_filter::filter::object_type();
		else if (status != ERROR_SUCCESS) {
			NSC_LOG_ERROR("Failed to read eventlog in real-time thread (resetting eventlog): " + error::lookup::last_error(status));
			reset_event(handle);
			return eventlog_filter::filter::object_type();
		}
	}
	return eventlog_filter::filter::object_type(new eventlog_filter::new_filter_obj(ltime, name, hEvents[0], hContext, 0));
}

//////////////////////////////////////////////////////////////////////////
// OLD API impl

eventlog_wrapper_old::eventlog_wrapper_old(const std::string &s_name) : hLog(NULL), buffer(1024*10), lastReadSize(0), nextBufferPosition(0) {
	name = eventlog_wrapper::find_eventlog_name(s_name);
	open();
}
eventlog_wrapper_old::~eventlog_wrapper_old() {
	if (isOpen())
		close();
	lastReadSize = 0;
	nextBufferPosition = 0;
}
void eventlog_wrapper_old::open() {
	hLog = OpenEventLog(NULL, utf8::cvt<std::wstring>(name).c_str());
	if (hLog == INVALID_HANDLE_VALUE) {
		throw nsclient::nsclient_exception("Failed to open eventlog: " + error::lookup::last_error());
	}
	seek_end();
}

void eventlog_wrapper_old::reopen() {
	if (isOpen())
		close();
	open();
}

void eventlog_wrapper_old::close() {
	if (!CloseEventLog(hLog)) {
		NSC_LOG_ERROR("Failed to close eventlog: " + error::lookup::last_error());
	}
}

bool eventlog_wrapper_old::get_last_record_number(DWORD* pdwRecordNumber) {
	DWORD OldestRecordNumber = 0;
	DWORD NumberOfRecords = 0;

	if (!GetOldestEventLogRecord(hLog, &OldestRecordNumber))
		return false;

	if (!GetNumberOfEventLogRecords(hLog, &NumberOfRecords))
		return false;

	*pdwRecordNumber = OldestRecordNumber + NumberOfRecords - 1;
	return true;
}

bool eventlog_wrapper_old::get_first_record_number(DWORD* pdwRecordNumber) {
	DWORD OldestRecordNumber = 0;

	if (!GetOldestEventLogRecord(hLog, &OldestRecordNumber))
		return false;

	*pdwRecordNumber = OldestRecordNumber;
	return true;
}

bool eventlog_wrapper_old::notify(HANDLE &handle) {
	handle = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!handle) {
		return false;
	}
	return NotifyChangeEventLog(hLog, handle) == TRUE;
}

bool eventlog_wrapper_old::un_notify(HANDLE &handle) {
	return CloseHandle(handle) == TRUE;
}

void eventlog_wrapper_old::reset_event(HANDLE &handle) {
}

bool eventlog_wrapper_old::seek_end() {
	DWORD dwLastRecordNumber = 0;

	if (!get_last_record_number(&dwLastRecordNumber))
		return false;

	if (do_record(dwLastRecordNumber, EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ) != ERROR_SUCCESS)
		return false;
	return true;
}

bool eventlog_wrapper_old::seek_start() {
	DWORD dwLastRecordNumber = 0;

	if (!get_first_record_number(&dwLastRecordNumber))
		return false;

	if (do_record(dwLastRecordNumber, EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ) != ERROR_SUCCESS)
		return false;
	return true;
}

void eventlog_wrapper_old::resize_buffer(DWORD size) {
	if (size <= buffer.size())
		return;
	buffer.resize(size);
}

eventlog_filter::filter::object_type eventlog_wrapper_old::read_record(HANDLE &handle) {
	DWORD status = do_record(0, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ);
	if (status != ERROR_SUCCESS  && status != ERROR_HANDLE_EOF) {
		NSC_LOG_MESSAGE("Assuming eventlog reset (re-reading from start)");
		un_notify(handle);
		reopen();
		notify(handle);
		seek_start();
	}

	__time64_t ltime;
	_time64(&ltime);


	if (nextBufferPosition >= lastReadSize) {
		if (do_record(0, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ) != ERROR_SUCCESS)
			return eventlog_filter::filter::object_type();
	}
	if (nextBufferPosition >= lastReadSize)
		return eventlog_filter::filter::object_type();
	EVENTLOGRECORD *pevlr = buffer.get(nextBufferPosition);
	if (pevlr == NULL)
		return eventlog_filter::filter::object_type();
	nextBufferPosition += pevlr->Length;
	return eventlog_filter::filter::object_type(new eventlog_filter::old_filter_obj(ltime, get_name(), pevlr, 0));
}


DWORD eventlog_wrapper_old::do_record(DWORD dwRecordNumber, DWORD dwFlags) {
	DWORD status = ERROR_SUCCESS;
	DWORD dwBytesToRead = buffer.size() - 10;
	lastReadSize = 0;
	nextBufferPosition = 0;
	DWORD dwMinimumBytesToRead = 0;

	if (!ReadEventLog(hLog, dwFlags, dwRecordNumber, (LPBYTE)buffer, dwBytesToRead, &lastReadSize, &dwMinimumBytesToRead)) {
		status = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == status) {
			status = ERROR_SUCCESS;
			buffer.resize(dwMinimumBytesToRead+20);
			dwBytesToRead = buffer.size() - 10;

			if (!ReadEventLog(hLog, dwFlags, dwRecordNumber, (LPBYTE)buffer, dwBytesToRead, &lastReadSize, &dwMinimumBytesToRead)) {
				status = GetLastError();
				NSC_LOG_ERROR_STD("Failed to read eventlog message: " + utf8::cvt<std::string>(error::lookup::last_error(status)));
				return status;
			}
		} else {
			if (ERROR_HANDLE_EOF != status) {
				NSC_LOG_ERROR_STD("Failed to read eventlog record(" + str::xtos(dwRecordNumber) + "): " + utf8::cvt<std::string>(error::lookup::last_error(status)));
				return status;
			}
		}
	}
	return status;
}


//////////////////////////////////////////////////////////////////////////
//
event_source::event_source(const std::wstring &name) : hLog(NULL) {
	open(L"", name);
}
event_source::~event_source() {
	if (isOpen())
		close();
}
void event_source::open(const std::wstring &server, const std::wstring &name) {
	if (server.empty())
		hLog = RegisterEventSource(NULL, name.c_str());
	else
		hLog = RegisterEventSource(server.c_str(), name.c_str());
}

void event_source::close() {
	DeregisterEventSource(hLog);
}