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

#include "simple_registry.hpp"

#include <nsclient/nsclient_exception.hpp>
#include <str/wstring.hpp>
#include <str/utils.hpp>

#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

class EventLogRecord : boost::noncopyable {
	const EVENTLOGRECORD *pevlr_;
	std::string file_;
public:
	EventLogRecord(std::string file, const EVENTLOGRECORD *pevlr) : file_(file), pevlr_(pevlr) {
		if (pevlr == NULL)
			throw nsclient::nsclient_exception("Invalid eventlog record");
	}
	inline unsigned long long generated() const {
		return pevlr_->TimeGenerated;
	}
	inline unsigned long long written() const {
		return pevlr_->TimeWritten;
	}
	inline WORD category() const {
		return pevlr_->EventCategory;
	}
	inline std::wstring get_source() const {
		return reinterpret_cast<const WCHAR*>(reinterpret_cast<const BYTE*>(pevlr_) + sizeof(EVENTLOGRECORD));
	}
	inline std::wstring get_computer() const {
		size_t len = wcslen(reinterpret_cast<const WCHAR*>(reinterpret_cast<const BYTE*>(pevlr_) + sizeof(EVENTLOGRECORD)));
		return reinterpret_cast<const WCHAR*>(reinterpret_cast<const BYTE*>(pevlr_) + sizeof(EVENTLOGRECORD) + (len + 1)*sizeof(wchar_t));
	}
	inline DWORD eventID() const {
		return (pevlr_->EventID & 0xffff);
	}
	inline DWORD severity() const {
		return (pevlr_->EventID >> 30) & 0x3;
	}
	inline DWORD facility() const {
		return (pevlr_->EventID >> 16) & 0xfff;
	}
	inline WORD customer() const {
		return (pevlr_->EventID >> 29) & 0x1;
	}
	inline DWORD raw_id() const {
		return pevlr_->EventID;
	}

	inline DWORD eventType() const {
		return pevlr_->EventType;
	}

	std::wstring userSID() const {
		if (pevlr_->UserSidOffset == 0)
			return L" ";
		PSID p = NULL; // = reinterpret_cast<const void*>(reinterpret_cast<const BYTE*>(pevlr_) + + pevlr_->UserSidOffset);
		DWORD userLen = 0;
		DWORD domainLen = 0;
		SID_NAME_USE sidName;

		LookupAccountSid(NULL, p, NULL, &userLen, NULL, &domainLen, &sidName);
		LPTSTR user = new TCHAR[userLen + 10];
		LPTSTR domain = new TCHAR[domainLen + 10];

		LookupAccountSid(NULL, p, user, &userLen, domain, &domainLen, &sidName);
		user[userLen] = 0;
		domain[domainLen] = 0;
		std::wstring ustr = user;
		std::wstring dstr = domain;
		delete[] user;
		delete[] domain;
		if (!dstr.empty())
			dstr = dstr + L"\\";
		if (ustr.empty() && dstr.empty())
			return L"missing";

		return dstr + ustr;
	}

	std::wstring enumStrings() const {
		std::wstring ret;
		const TCHAR* p = reinterpret_cast<const TCHAR*>(reinterpret_cast<const BYTE*>(pevlr_) + pevlr_->StringOffset);
		for (unsigned int i = 0; i < pevlr_->NumStrings; i++) {
			std::wstring s = p;
			if (!s.empty())
				s += L", ";
			ret += s;
			p = &p[wcslen(p) + 1];
		}
		return ret;
	}

	static WORD appendType(WORD dwType, std::wstring sType) {
		return dwType | translateType(sType);
	}
	static WORD subtractType(WORD dwType, std::wstring sType) {
		return dwType & (!translateType(sType));
	}
	static WORD translateType(std::wstring sType) {
		if (sType.empty())
			return EVENTLOG_ERROR_TYPE;
		if (sType == L"error")
			return EVENTLOG_ERROR_TYPE;
		if (sType == L"warning")
			return EVENTLOG_WARNING_TYPE;
		if (sType == L"success")
			return EVENTLOG_SUCCESS;
		if (sType == L"info")
			return EVENTLOG_INFORMATION_TYPE;
		if (sType == L"auditSuccess")
			return EVENTLOG_AUDIT_SUCCESS;
		if (sType == L"auditFailure")
			return EVENTLOG_AUDIT_FAILURE;
		return static_cast<WORD>(strEx::stox<WORD>(sType));
	}
	static WORD translateType(std::string sType) {
		if (sType.empty())
			return EVENTLOG_ERROR_TYPE;
		if (sType == "error")
			return EVENTLOG_ERROR_TYPE;
		if (sType == "warning")
			return EVENTLOG_WARNING_TYPE;
		if (sType == "success")
			return EVENTLOG_SUCCESS;
		if (sType == "info")
			return EVENTLOG_INFORMATION_TYPE;
		if (sType == "auditSuccess")
			return EVENTLOG_AUDIT_SUCCESS;
		if (sType == "auditFailure")
			return EVENTLOG_AUDIT_FAILURE;
		return str::stox<WORD>(sType);
	}
	static std::wstring translateType(WORD dwType) {
		if (dwType == EVENTLOG_ERROR_TYPE)
			return L"error";
		if (dwType == EVENTLOG_WARNING_TYPE)
			return L"warning";
		if (dwType == EVENTLOG_SUCCESS)
			return L"success";
		if (dwType == EVENTLOG_INFORMATION_TYPE)
			return L"info";
		if (dwType == EVENTLOG_AUDIT_SUCCESS)
			return L"auditSuccess";
		if (dwType == EVENTLOG_AUDIT_FAILURE)
			return L"auditFailure";
		return strEx::xtos(dwType);
	}
	static WORD translateSeverity(std::wstring sType) {
		if (sType.empty())
			return 0;
		if (sType == L"success")
			return 0;
		if (sType == L"informational")
			return 1;
		if (sType == L"warning")
			return 2;
		if (sType == L"error")
			return 3;
		return static_cast<WORD>(strEx::stox<WORD>(sType));
	}
	static WORD translateSeverity(std::string sType) {
		if (sType.empty())
			return 0;
		if (sType == "success")
			return 0;
		if (sType == "informational")
			return 1;
		if (sType == "warning")
			return 2;
		if (sType == "error")
			return 3;
		return str::stox<WORD>(sType);
	}
	static std::wstring translateSeverity(WORD dwType) {
		if (dwType == 0)
			return L"success";
		if (dwType == 1)
			return L"informational";
		if (dwType == 2)
			return L"warning";
		if (dwType == 3)
			return L"error";
		return strEx::xtos(dwType);
	}
	bool get_dll(std::wstring &file_or_error) const {
		try {
			file_or_error = simple_registry::registry_key::get_string(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\" + utf8::cvt<std::wstring>(file_) + (std::wstring)L"\\" + get_source(), L"EventMessageFile");
			return true;
		} catch (simple_registry::registry_exception &) {
		}
		try {
			std::wstring providerGuid = simple_registry::registry_key::get_string(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\" + utf8::cvt<std::wstring>(file_) + (std::wstring)L"\\" + get_source(), L"ProviderGuid");
			file_or_error = simple_registry::registry_key::get_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Publishers" + providerGuid, L"MessageFileName");
			return true;
		} catch (simple_registry::registry_exception &e) {
			file_or_error = L"Could not extract DLL for eventsource: " + get_source() + L": " + utf8::cvt<std::wstring>(e.reason());
			return false;
		}
	}

	struct tchar_array {
		TCHAR **buffer;
		std::size_t size;
		tchar_array(std::size_t size) : buffer(NULL), size(size) {
			buffer = new TCHAR*[size];
			for (std::size_t i = 0; i < size; i++)
				buffer[i] = NULL;
		}
		~tchar_array() {
			for (std::size_t i = 0; i < size; i++)
				delete[] buffer[i];
			delete[] buffer;
		}
		std::size_t set(std::size_t i, const TCHAR* str) {
			std::size_t len = wcslen(str);
			buffer[i] = new TCHAR[len + 2];
			wcsncpy(buffer[i], str, len + 1);
			return len;
		}
		TCHAR** get_buffer_unsafe() { return buffer; }
	};

	boost::tuple<DWORD, std::wstring> safe_format(HMODULE hDLL, DWORD dwLang) const {
		LPVOID lpMsgBuf;
		unsigned long dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_IGNORE_INSERTS, hDLL,
			pevlr_->EventID, dwLang, (LPTSTR)&lpMsgBuf, 0, NULL);
		if (dwRet == 0) {
			return boost::tuple<DWORD, std::wstring>(GetLastError(), L" ");
		}
		std::wstring msg = reinterpret_cast<wchar_t*>(lpMsgBuf);
		LocalFree(lpMsgBuf);
		const TCHAR* p = reinterpret_cast<const TCHAR*>(reinterpret_cast<const BYTE*>(pevlr_) + pevlr_->StringOffset);
		for (unsigned int i = 0; i < pevlr_->NumStrings; i++) {
			strEx::replace(msg, L"%" + strEx::xtos(i + 1), std::wstring(p));
			std::size_t len = wcslen(p);
			p = &(p[len + 1]);
		}
		return boost::make_tuple(0, msg);
	}
	std::wstring render_message(const int truncate_message, DWORD dwLang = 0) const {
		std::vector<std::wstring> args;
		std::wstring ret;
		std::wstring file;
		if (!get_dll(file)) {
			return file;
		}
		BOOST_FOREACH(const std::wstring &dll, strEx::splitEx(file, L";")) {
			//std::wstring msg = error::format::message::from_module((*cit), eventID(), _sz);
			std::wstring msg;
			try {
				HMODULE hDLL = LoadLibraryEx(dll.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
				if (hDLL == NULL) {
					msg = L"failed to load: " + dll + L", reason: " + strEx::xtos(GetLastError());
					continue;
				}
				if (dwLang == 0)
					dwLang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
				boost::tuple<DWORD, std::wstring> formated_data = safe_format(hDLL, dwLang);
				if (formated_data.get<0>() != 0) {
					FreeLibrary(hDLL);
					if (formated_data.get<0>() == 15100) {
						// Invalid MUI file (wrong language)
						msg = L" ";
						continue;
					}
					if (formated_data.get<0>() == 317) {
						// Missing message
						msg = L" ";
						continue;
					}
					msg = L"failed to lookup error code: " + strEx::xtos(eventID()) + L" from DLL: " + dll + L"( reason: " + strEx::xtos(formated_data.get<0>()) + L")";
					continue;
				}
				FreeLibrary(hDLL);
				msg = formated_data.get<1>();
			} catch (...) {
				msg = L"Unknown exception getting message";
			}
			strEx::replace(msg, L"\n", L" ");
			strEx::replace(msg, L"\t", L" ");
			std::string::size_type pos = msg.find_last_not_of(L"\n\t ");
			if (pos != std::string::npos) {
				msg = msg.substr(0, pos);
			}
			if (!msg.empty()) {
				if (!ret.empty())
					ret += L", ";
				ret += msg;
			}
		}
		if (truncate_message > 0 && ret.length() > truncate_message)
			ret = ret.substr(0, truncate_message);
		return ret;
	}
	SYSTEMTIME get_time(DWORD time) const {
		FILETIME FileTime, LocalFileTime;
		SYSTEMTIME SysTime;
		__int64 lgTemp;
		__int64 SecsTo1970 = 116444736000000000;

		lgTemp = Int32x32To64(time, 10000000) + SecsTo1970;

		FileTime.dwLowDateTime = (DWORD)lgTemp;
		FileTime.dwHighDateTime = (DWORD)(lgTemp >> 32);

		FileTimeToLocalFileTime(&FileTime, &LocalFileTime);
		FileTimeToSystemTime(&LocalFileTime, &SysTime);
		return SysTime;
	}

	SYSTEMTIME get_time_generated() const {
		return get_time(pevlr_->TimeGenerated);
	}
	SYSTEMTIME get_time_written() const {
		return get_time(pevlr_->TimeWritten);
	}
	inline std::string get_log() const {
		return file_;
	}
};