#include "stdafx.h"
#include "eventlog_wrapper.hpp"
#include "simple_registry.hpp"

eventlog_wrapper::eventlog_wrapper(const std::string &s_name) : name(s_name), hLog(NULL), pBuffer(NULL), bufferSize(0), lastReadSize(0) {
	open(name);
}
eventlog_wrapper::~eventlog_wrapper() {
	if (isOpen())
		close();
	delete [] pBuffer;
	bufferSize = 0;
	lastReadSize = 0;
}
void eventlog_wrapper::open(const std::string &name) {
	std::string realname = find_eventlog_name(name);
	hLog = OpenEventLog(NULL, utf8::cvt<std::wstring>(realname).c_str());
}

void eventlog_wrapper::close() {
	CloseEventLog(hLog);
}

bool eventlog_wrapper::get_last_Record_number(DWORD* pdwRecordNumber) {
	DWORD status = ERROR_SUCCESS;
	DWORD OldestRecordNumber = 0;
	DWORD NumberOfRecords = 0;

	if (!GetOldestEventLogRecord(hLog, &OldestRecordNumber))
		return false;

	if (!GetNumberOfEventLogRecords(hLog, &NumberOfRecords))
		return false;

	*pdwRecordNumber = OldestRecordNumber + NumberOfRecords - 1;
	return true;
}

bool eventlog_wrapper::notify(HANDLE &handle) {
	handle = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!handle) {
		return false;
	}
	return NotifyChangeEventLog(hLog, handle);
}

bool eventlog_wrapper::seek_end() {
	DWORD dwLastRecordNumber = 0;

	if (!get_last_Record_number(&dwLastRecordNumber))
		return false;

	if (read_record(dwLastRecordNumber, EVENTLOG_SEEK_READ | EVENTLOG_FORWARDS_READ) != ERROR_SUCCESS)
		return false;
	return true;
}

void eventlog_wrapper::resize_buffer(int size) {
	if (size <= bufferSize)
		return;
	PBYTE tmp = pBuffer;
	pBuffer = new BYTE[size];
	bufferSize = size;
	delete tmp;
}


std::string eventlog_wrapper::find_eventlog_name(const std::string name) {
	try {
		simple_registry::registry_key key(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog"));
		BOOST_FOREACH(const std::wstring k, key.get_keys()) {
			try {
				simple_registry::registry_key sub_key(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\") + k);
				std::wstring file = sub_key.get_string(_T("DisplayNameFile"));
				int id = sub_key.get_int(_T("DisplayNameID"));
				std::string real_name = error::format::message::from_module(utf8::cvt<std::string>(file), id);
				strEx::replace(real_name, "\n", "");
				strEx::replace(real_name, "\r", "");
				if (real_name == name)
					return utf8::cvt<std::string>(k);
			} catch (simple_registry::registry_exception &e) { e;}
		}
		return name;
	} catch (simple_registry::registry_exception &e) {
		return name;
	} catch (...) {
		return name;
	}
}
EVENTLOGRECORD* eventlog_wrapper::read_record_with_buffer() {
	if (nextBufferPosition >= lastReadSize) {
		if (read_record(0, EVENTLOG_SEQUENTIAL_READ|EVENTLOG_FORWARDS_READ) != ERROR_SUCCESS)
			return NULL;
	}
	if (nextBufferPosition >= lastReadSize)
		return NULL;
	EVENTLOGRECORD *pevlr = reinterpret_cast<EVENTLOGRECORD*>(&pBuffer[nextBufferPosition]);
	nextBufferPosition += pevlr->Length; 
	return pevlr;
}

DWORD eventlog_wrapper::read_record(DWORD dwRecordNumber, DWORD dwFlags) {
	resize_buffer(sizeof(EVENTLOGRECORD));
	DWORD status = ERROR_SUCCESS;
	DWORD dwBytesToRead = bufferSize;
	lastReadSize = 0;
	nextBufferPosition = 0;
	DWORD dwMinimumBytesToRead = 0;

	if (!ReadEventLog(hLog, dwFlags, dwRecordNumber, pBuffer, dwBytesToRead, &lastReadSize, &dwMinimumBytesToRead)) {
		status = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == status) {
			status = ERROR_SUCCESS;
			resize_buffer(dwMinimumBytesToRead);
			dwBytesToRead = bufferSize;

			if (!ReadEventLog(hLog, dwFlags, dwRecordNumber, pBuffer, dwBytesToRead, &lastReadSize, &dwMinimumBytesToRead)) {
				status = GetLastError();
				NSC_LOG_ERROR_STD("Failed to read eventlog message: " + utf8::cvt<std::string>(error::lookup::last_error(status)));
				return status;
			}
		} else {
			if (ERROR_HANDLE_EOF != status)	{
				NSC_LOG_ERROR_STD("Failed to read eventlog record(" + strEx::s::xtos(dwRecordNumber) + "): " + utf8::cvt<std::string>(error::lookup::last_error(status)));
				return status;
			}
		}
	}
	return status;
}


event_source::event_source(const std::wstring &name) : hLog(NULL) {
	open(_T(""), name);
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


