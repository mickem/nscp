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

#pragma once

#include <string>

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include "modern_eventlog.hpp"
#include "filter.hpp"


struct eventlog_wrapper : boost::noncopyable {

	virtual void open() = 0;
	virtual void reopen() = 0;
	virtual void close() = 0;

	virtual std::string get_name() const = 0;

	virtual bool notify(HANDLE &handle) = 0;
	virtual bool un_notify(HANDLE &handle) = 0;
	virtual void reset_event(HANDLE &handle) = 0;
	virtual eventlog_filter::filter::object_type read_record(HANDLE &handle) = 0;

	static std::string find_eventlog_name(std::string name);
};

struct eventlog_wrapper_new : public eventlog_wrapper {
	eventlog::evt_handle hLog;
	eventlog::evt_handle hContext;
	std::string name;
	bool is_new;
	eventlog_wrapper_new(const std::string &name);
	~eventlog_wrapper_new();

	void open();
	void reopen();
	void close();
	bool isOpen() {
		return hLog != NULL;
	}
	std::string get_name() const { return name; }

	bool notify(HANDLE &handle);
	bool un_notify(HANDLE &handle);
	void reset_event(HANDLE &handle);
	eventlog_filter::filter::object_type read_record(HANDLE &handle);
};


struct eventlog_wrapper_old : public eventlog_wrapper {
private:
	HANDLE hLog;
	hlp::buffer<BYTE, EVENTLOGRECORD*> buffer;
	DWORD lastReadSize;
	DWORD nextBufferPosition;
	std::string name;

public:
	eventlog_wrapper_old(const std::string &name);
	~eventlog_wrapper_old();

	void open();
	void reopen();
	void close();
	bool isOpen() {
		return hLog != NULL;
	}
	std::string get_name() const { return name; }

	bool notify(HANDLE &handle);
	bool un_notify(HANDLE &handle);
	void reset_event(HANDLE &handle);
	DWORD do_record(DWORD dwRecordNumber, DWORD dwFlags);
	eventlog_filter::filter::object_type read_record(HANDLE &handle);


private:
	bool seek_end();
	bool seek_start();
	bool get_last_record_number(DWORD* pdwRecordNumber);
	bool get_first_record_number(DWORD* pdwRecordNumber);
	DWORD get_last_buffer_size() {
		return lastReadSize;
	}
	void resize_buffer(DWORD size);

};

struct event_source {
	HANDLE hLog;
	event_source(const std::wstring &source);
	~event_source();

	void open(const std::wstring &server, const std::wstring &name);
	void close();
	bool isOpen() {
		return hLog != NULL;
	}

	operator HANDLE () {
		return hLog;
	}
};