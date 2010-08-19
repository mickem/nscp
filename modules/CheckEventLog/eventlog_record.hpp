#pragma once

class EventLogRecord {
	EVENTLOGRECORD *pevlr_;
	__int64 currentTime_;
	std::wstring file_;
public:
	EventLogRecord(std::wstring file, EVENTLOGRECORD *pevlr, __int64 currentTime) : file_(file), pevlr_(pevlr), currentTime_(currentTime) {
	}
	inline __int64 timeGenerated() const {
		return (currentTime_-pevlr_->TimeGenerated)*1000;
	}
	inline __int64 timeWritten() const {
		return (currentTime_-pevlr_->TimeWritten)*1000;
	}
	inline __int64 generated() const {
		return pevlr_->TimeGenerated;
	}
	inline __int64 written() const {
		return pevlr_->TimeWritten;
	}
	inline std::wstring eventSource() const {
		return reinterpret_cast<WCHAR*>(reinterpret_cast<LPBYTE>(pevlr_) + sizeof(EVENTLOGRECORD));
	}
	inline DWORD eventID() const {
		return (pevlr_->EventID&0xffff);
	}
	inline DWORD severity() const {
		return (pevlr_->EventID>>30);
	}

	inline DWORD eventType() const {
		return pevlr_->EventType;
	}

	std::wstring userSID() const {
		if (pevlr_->UserSidOffset == 0)
			return _T("");
		PSID p = reinterpret_cast<PSID>(reinterpret_cast<LPBYTE>(pevlr_) + + pevlr_->UserSidOffset);
		DWORD userLen = 0;
		DWORD domainLen = 0;
		SID_NAME_USE sidName;

		LookupAccountSid(NULL, p, NULL, &userLen, NULL, &domainLen, &sidName);
		LPTSTR user = new TCHAR[userLen+10];
		LPTSTR domain = new TCHAR[domainLen+10];

		LookupAccountSid(NULL, p, user, &userLen, domain, &domainLen, &sidName);
		user[userLen] = 0;
		domain[domainLen] = 0;
		std::wstring ustr = user;
		std::wstring dstr = domain;
		delete [] user;
		delete [] domain;
		if (!dstr.empty())
			dstr = dstr + _T("\\");
		if (ustr.empty() && dstr.empty())
			return _T("missing");

		return dstr + ustr;
	}

	std::wstring enumStrings() const {
		std::wstring ret;
		TCHAR* p = reinterpret_cast<TCHAR*>(reinterpret_cast<LPBYTE>(pevlr_) + pevlr_->StringOffset);
		for (unsigned int i =0;i<pevlr_->NumStrings;i++) {
			std::wstring s = p;
			if (!s.empty())
				s += _T(", ");
			ret += s;
			p+= wcslen(p)+1;
		}
		return ret;
	}

	static DWORD appendType(DWORD dwType, std::wstring sType) {
		return dwType | translateType(sType);
	}
	static DWORD subtractType(DWORD dwType, std::wstring sType) {
		return dwType & (!translateType(sType));
	}
	static DWORD translateType(std::wstring sType) {
		if (sType == _T("error"))
			return EVENTLOG_ERROR_TYPE;
		if (sType == _T("warning"))
			return EVENTLOG_WARNING_TYPE;
		if (sType == _T("info"))
			return EVENTLOG_INFORMATION_TYPE;
		if (sType == _T("auditSuccess"))
			return EVENTLOG_AUDIT_SUCCESS;
		if (sType == _T("auditFailure"))
			return EVENTLOG_AUDIT_FAILURE;
		return strEx::stoi(sType);
	}
	static std::wstring translateType(DWORD dwType) {
		if (dwType == EVENTLOG_ERROR_TYPE)
			return _T("error");
		if (dwType == EVENTLOG_WARNING_TYPE)
			return _T("warning");
		if (dwType == EVENTLOG_INFORMATION_TYPE)
			return _T("info");
		if (dwType == EVENTLOG_AUDIT_SUCCESS)
			return _T("auditSuccess");
		if (dwType == EVENTLOG_AUDIT_FAILURE)
			return _T("auditFailure");
		return strEx::itos(dwType);
	}
	static DWORD translateSeverity(std::wstring sType) {
		if (sType == _T("success"))
			return 0;
		if (sType == _T("informational"))
			return 1;
		if (sType == _T("warning"))
			return 2;
		if (sType == _T("error"))
			return 3;
		return strEx::stoi(sType);
	}
	static std::wstring translateSeverity(DWORD dwType) {
		if (dwType == 0)
			return _T("success");
		if (dwType == 1)
			return _T("informational");
		if (dwType == 2)
			return _T("warning");
		if (dwType == 3)
			return _T("error");
		return strEx::itos(dwType);
	}
	std::wstring get_dll() {
		try {
			return simple_registry::get_string(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\") + file_ + (std::wstring)_T("\\") + eventSource(), _T("EventMessageFile"));
		} catch (simple_registry::registry_exception &e) {
			NSC_LOG_ERROR_STD(_T("Could not extract DLL for eventsource: ") + eventSource() + _T(": ") + e.what());
			return _T("");
		}
	}

	std::wstring render_message() {
		std::vector<std::wstring> args;
		TCHAR* *pArgs = new TCHAR*[pevlr_->NumStrings+1];
		TCHAR* p = reinterpret_cast<TCHAR*>(reinterpret_cast<LPBYTE>(pevlr_) + pevlr_->StringOffset);
		for (unsigned int i =0;i<pevlr_->NumStrings;i++) {
			args.push_back(p);
			pArgs[i] = p;
			DWORD len = (DWORD)wcslen(p);
			p = &(p[len+1]);
			//p += len+1;
		}

		std::wstring ret;
		strEx::splitList dlls = strEx::splitEx(get_dll(), _T(";"));
		for (strEx::splitList::const_iterator cit = dlls.begin(); cit != dlls.end(); ++cit) {
			//std::wstring msg = error::format::message::from_module((*cit), eventID(), _sz);
			std::wstring msg;
			try {
				msg = error::format::message::from_module_x64((*cit), eventID(), pArgs, pevlr_->NumStrings);
				if (msg.empty()) {
					msg = error::format::message::from_module_x64((*cit), pevlr_->EventID, pArgs, pevlr_->NumStrings);
				}
			} catch (...) {
				msg = _T("Unknown exception getting message");
			}
			strEx::replace(msg, _T("\n"), _T(" "));
			strEx::replace(msg, _T("\t"), _T(" "));
			std::string::size_type pos = msg.find_last_not_of(_T("\n\t "));
			if (pos != std::string::npos) {
				msg = msg.substr(0,pos);
			}
			if (!msg.empty()) {
				if (!ret.empty())
					ret += _T(", ");
				ret += msg;
			}
		}
		delete [] pArgs;
		return ret;
	}
	SYSTEMTIME get_time(DWORD time) {
		FILETIME FileTime, LocalFileTime;
		SYSTEMTIME SysTime;
		__int64 lgTemp;
		__int64 SecsTo1970 = 116444736000000000;

		lgTemp = Int32x32To64(time,10000000) + SecsTo1970;

		FileTime.dwLowDateTime = (DWORD) lgTemp;
		FileTime.dwHighDateTime = (DWORD)(lgTemp >> 32);

		FileTimeToLocalFileTime(&FileTime, &LocalFileTime);
		FileTimeToSystemTime(&LocalFileTime, &SysTime);
		return SysTime;
	}

	SYSTEMTIME get_time_generated() {
		return get_time(pevlr_->TimeGenerated);
	}
	SYSTEMTIME get_time_written() {
		return get_time(pevlr_->TimeWritten);
	}

	inline boost::posix_time::ptime systemtime_to_time(const SYSTEMTIME &time) {
		struct tm tmTime;
		memset(&tmTime, 0, sizeof(tmTime));

		tmTime.tm_sec = time.wSecond; // seconds after the minute - [0,59]
		tmTime.tm_min = time.wMinute; // minutes after the hour - [0,59]
		tmTime.tm_hour = time.wHour;  // hours since midnight - [0,23]
		tmTime.tm_mday = time.wDay;  // day of the month - [1,31]
		tmTime.tm_mon = time.wMonth-1; // months since January - [0,11]
		tmTime.tm_year = time.wYear-1900; // years since 1900
		tmTime.tm_wday = time.wDayOfWeek; // days since Sunday - [0,6]

		return boost::posix_time::ptime_from_tm(tmTime);
	}

	std::wstring render(bool propper, std::wstring syntax, std::wstring date_format = DATE_FORMAT) {
		if (propper) {
			// To obtain the appropriate message string from the message file, load the message file with the LoadLibrary function and use the FormatMessage function
			strEx::replace(syntax, _T("%message%"), render_message());
		} else {
			strEx::replace(syntax, _T("%message%"), _T("%message% needs the descriptions flag set!"));
		}

		strEx::replace(syntax, _T("%source%"), eventSource());
		strEx::replace(syntax, _T("%generated%"), strEx::format_date(systemtime_to_time(get_time_generated()), date_format));
		strEx::replace(syntax, _T("%written%"), strEx::format_date(systemtime_to_time(get_time_written()), date_format));
		strEx::replace(syntax, _T("%generated-raw%"), strEx::itos(pevlr_->TimeGenerated));
		strEx::replace(syntax, _T("%written-raw%"), strEx::itos(pevlr_->TimeWritten));
		strEx::replace(syntax, _T("%type%"), translateType(eventType()));
		strEx::replace(syntax, _T("%severity%"), translateSeverity(severity()));
		strEx::replace(syntax, _T("%strings%"), enumStrings());
		strEx::replace(syntax, _T("%id%"), strEx::itos(eventID()));
		strEx::replace(syntax, _T("%user%"), userSID());
		return syntax;
	}
};