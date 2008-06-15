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
#include "CheckEventLog.h"
#include <filter_framework.hpp>

#include <strEx.h>
#include <time.h>
#include <utils.h>
#include <error.hpp>
#include <map>

CheckEventLog gCheckEventLog;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

CheckEventLog::CheckEventLog() {
}
CheckEventLog::~CheckEventLog() {
}


bool CheckEventLog::loadModule(NSCAPI::moduleLoadMode mode) {
	try {

		if (SETTINGS_GET_BOOL(settings_def::COMPATIBLITY)) {
			NSC_DEBUG_MSG(_T("Using compatiblity mode in: EventLog Checker"));
#define EVENTLOG_SECTION_TITLE _T("Eventlog")
#define EVENTLOG_DEBUG _T("debug")
#define EVENTLOG_SYNTAX _T("syntax")
			SETTINGS_MAP_KEY_A(event_log::DEBUG_KEY,	EVENTLOG_SECTION_TITLE, EVENTLOG_DEBUG);
			SETTINGS_MAP_KEY_A(event_log::SYNTAX,		EVENTLOG_SECTION_TITLE, EVENTLOG_SYNTAX);
		}
		SETTINGS_REG_PATH(event_log::SECTION);
		SETTINGS_REG_KEY_B(event_log::DEBUG_KEY);
		SETTINGS_REG_KEY_S(event_log::SYNTAX);

		NSCModuleHelper::registerCommand(_T("CheckEventLog"), _T("Check for errors in the event logger!"));
		debug_ = SETTINGS_GET_BOOL(event_log::DEBUG_KEY);
		syntax_ = SETTINGS_GET_STRING(event_log::SYNTAX);
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	return true;
}
bool CheckEventLog::unloadModule() {
	return true;
}

bool CheckEventLog::hasCommandHandler() {
	return true;
}
bool CheckEventLog::hasMessageHandler() {
	return false;
}


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
/*
	std::wstring userSID() const {
		if (pevlr_->UserSidOffset == 0)
			return "";
		PSID p = reinterpret_cast<PSID>(reinterpret_cast<LPBYTE>(pevlr_) + + pevlr_->UserSidOffset);
		LPSTR user = new CHAR[1025];
		LPSTR domain = new CHAR[1025];
		DWORD userLen = 1024;
		DWORD domainLen = 1024;
		SID_NAME_USE sidName;
		LookupAccountSid(NULL, p, user, &userLen, domain, &domainLen, &sidName);
		user[userLen] = 0;
		domain[domainLen] = 0;
		return std::wstring(domain) + "\\" + std::wstring(user);
	}
	*/

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
		HKEY hKey = HKEY_LOCAL_MACHINE;
		std::wstring path = ((std::wstring)_T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\") + file_ + (std::wstring)_T("\\") + eventSource()).c_str();
		std::wstring ret;
		HKEY hTemp;
		LONG lRet = ERROR_SUCCESS;
		if (lRet = RegOpenKeyEx(hKey, path.c_str(), 0, KEY_QUERY_VALUE, &hTemp) != ERROR_SUCCESS) {
			NSC_LOG_ERROR_STD(_T("Could not extract DLL for eventsource: ") + eventSource() + error::format::from_system(lRet));
			return ret;
		}
		DWORD type;
		const DWORD data_length = 2048;
		DWORD cbData = data_length;
		BYTE *bData = new BYTE[cbData];
		lRet = RegQueryValueEx(hTemp, _T("EventMessageFile"), NULL, &type, bData, &cbData);
		if (lRet == ERROR_SUCCESS) {
			if (type == REG_SZ) {
				if (cbData < data_length-1) {
					bData[cbData] = 0;
					ret = reinterpret_cast<LPCTSTR>(bData);
				} else {
					NSC_LOG_ERROR_STD(_T("Could not extract DLL for eventsource: ") + eventSource());
				}
			} else if (type == REG_EXPAND_SZ) {
#define EXPAND_BUFFER_SIZE 2048
				if (cbData < data_length-1) {
					bData[cbData] = 0;
					std::wstring s = reinterpret_cast<LPCTSTR>(bData);
					TCHAR *buffer = new TCHAR[EXPAND_BUFFER_SIZE+1];
					DWORD expRet = ExpandEnvironmentStrings(s.c_str(), buffer, EXPAND_BUFFER_SIZE);
					if (expRet >= EXPAND_BUFFER_SIZE)
						NSC_LOG_ERROR_STD(_T("Could not extract DLL for eventsource: ") + eventSource());
					else
						ret = buffer;
				} else {
					NSC_LOG_ERROR_STD(_T("Could not extract DLL for eventsource: ") + eventSource());
				}
			} else {
				NSC_LOG_ERROR_STD(_T("Could not extract DLL for eventsource: ") + eventSource());
			}
		} else {
			NSC_LOG_ERROR_STD(_T("Could not extract DLL for eventsource: ") + eventSource() + error::format::from_system(lRet));
		}
		RegCloseKey(hTemp);
		delete [] bData;
		return ret;
	}

	std::wstring render_message() {
		DWORD *dwArgs = new DWORD[pevlr_->NumStrings+1];
		TCHAR* p = reinterpret_cast<TCHAR*>(reinterpret_cast<LPBYTE>(pevlr_) + pevlr_->StringOffset);
		for (unsigned int i =0;i<pevlr_->NumStrings;i++) {
			dwArgs[i] = reinterpret_cast<DWORD>(p);
			DWORD len = wcslen(p);
			p += len+1;
		}

/*
		TCHAR **_sz = (TCHAR**)GlobalAlloc(GPTR, (pevlr_->NumStrings)*sizeof(TCHAR *));
		register UINT z;
		TCHAR* p = reinterpret_cast<TCHAR*>(reinterpret_cast<LPBYTE>(pevlr_) + pevlr_->StringOffset);
		for(unsigned int z = 0; z < pevlr_->NumStrings; z++) {
			DWORD len = wcslen(p);
			_sz[z] = (TCHAR *)GlobalAlloc(GPTR, (len+1) * sizeof(TCHAR));
			wcscpy_s(_sz[z], len, p);
			p += len+1;
		}
*/
		std::wstring ret;
		strEx::splitList dlls = strEx::splitEx(get_dll(), _T(";"));
		for (strEx::splitList::const_iterator cit = dlls.begin(); cit != dlls.end(); ++cit) {
			//std::wstring msg = error::format::message::from_module((*cit), eventID(), _sz);
			std::wstring msg = error::format::message::from_module((*cit), eventID(), dwArgs);
			if (msg.empty()) {
				msg = error::format::message::from_module((*cit), pevlr_->EventID, dwArgs);
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
		delete [] dwArgs;
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

	std::wstring render(bool propper, std::wstring syntax, std::wstring date_format = DATE_FORMAT) {
		if (propper) {
			// To obtain the appropriate message string from the message file, load the message file with the LoadLibrary function and use the FormatMessage function
			strEx::replace(syntax, _T("%message%"), render_message());
		} else {
			strEx::replace(syntax, _T("%message%"), _T("%message% needs the descriptions flag set!"));
		}

		strEx::replace(syntax, _T("%source%"), eventSource());
		strEx::replace(syntax, _T("%generated%"), strEx::format_date(get_time_generated(), date_format));
		strEx::replace(syntax, _T("%written%"), strEx::format_date(get_time_written(), date_format));
		strEx::replace(syntax, _T("%type%"), translateType(eventType()));
		strEx::replace(syntax, _T("%severity%"), translateSeverity(severity()));
		strEx::replace(syntax, _T("%strings%"), enumStrings());
		strEx::replace(syntax, _T("%id%"), strEx::itos(eventID()));
		return syntax;
	}
};
/*
return (pevlr_->EventID&0xffff);
}
inline DWORD severity() const {
return (pevlr_->EventID>>30);
*/
class uniq_eventlog_record {
	DWORD ID;
	WORD type;
	WORD category;
public:
	std::wstring message;
	uniq_eventlog_record(EVENTLOGRECORD *pevlr) : ID(pevlr->EventID&0xffff), type(pevlr->EventType), category(pevlr->EventCategory) {}
	bool operator< (const uniq_eventlog_record &other) const { 
		return (ID < other.ID) || ((ID==other.ID)&&(type < other.type)) || (ID==other.ID&&type==other.type)&&(category < other.category);
	}
	std::wstring to_string() const {
		return _T("id=") + strEx::itos(ID) + _T("type=") + strEx::itos(type) + _T("category=") + strEx::itos(category);
	}
};
typedef std::map<uniq_eventlog_record,unsigned int> uniq_eventlog_map;


struct eventlog_filter {
	filters::filter_all_strings eventSource;
	filters::filter_all_numeric<unsigned int, filters::handlers::eventtype_handler> eventType;
	filters::filter_all_numeric<unsigned int, filters::handlers::eventseverity_handler> eventSeverity;
	filters::filter_all_strings message;
	filters::filter_all_times timeWritten;
	filters::filter_all_times timeGenerated;
	filters::filter_all_numeric<DWORD, filters::handlers::eventtype_handler> eventID;
	std::wstring value_;

	inline bool hasFilter() {
		return eventSource.hasFilter() || eventType.hasFilter() || eventID.hasFilter() || eventSeverity.hasFilter() || message.hasFilter() || 
			timeWritten.hasFilter() || timeGenerated.hasFilter();
	}
	std::wstring getValue() const {
		if (eventSource.hasFilter())
			return _T("event-source: ") + eventSource.getValue();
		if (eventType.hasFilter())
			return _T("event-type: ") + eventType.getValue();
		if (eventSeverity.hasFilter())
			return _T("severity: ") + eventSeverity.getValue();
		if (eventID.hasFilter())
			return _T("event-id: ") + eventID.getValue();
		if (message.hasFilter())
			return _T("message: ") + message.getValue();
		if (timeWritten.hasFilter())
			return _T("time-written: ") + timeWritten.getValue();
		if (timeGenerated.hasFilter())
			return _T("time-generated: ") + timeGenerated.getValue();
		return _T("UNknown...");
	}
	bool matchFilter(const EventLogRecord &value) const {
		if ((eventSource.hasFilter())&&(eventSource.matchFilter(value.eventSource())))
			return true;
		else if ((eventType.hasFilter())&&(eventType.matchFilter(value.eventType())))
			return true;
		else if ((eventSeverity.hasFilter())&&(eventSeverity.matchFilter(value.severity())))
			return true;
		else if ((eventID.hasFilter())&&(eventID.matchFilter(value.eventID()))) 
			return true;
		else if ((message.hasFilter())&&(message.matchFilter(value.enumStrings())))
			return true;
		else if ((timeWritten.hasFilter())&&(timeWritten.matchFilter(value.timeWritten())))
			return true;
		else if ((timeGenerated.hasFilter())&&(timeGenerated.matchFilter(value.timeGenerated())))
			return true;
		return false;
	}
};


#define MAP_FILTER(value, obj, filtermode) \
			else if (p__.first == value) { eventlog_filter filter; filter.obj = p__.second; filter_chain.push_back(filteritem_type(filtermode, filter)); }


#define BUFFER_SIZE 1024*64
NSCAPI::nagiosReturn CheckEventLog::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	if (command != _T("CheckEventLog"))
		return NSCAPI::returnIgnored;
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsULongInteger> EventLogQueryConatiner;
	typedef std::pair<int,eventlog_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	std::list<std::wstring> files;
	filterlist_type filter_chain;
	EventLogQueryConatiner query;

	bool bPerfData = true;
	bool bFilterIn = true;
	bool bFilterAll = false;
	bool bFilterNew = false;
	bool bShowDescriptions = false;
	bool unique = false;
	unsigned int truncate = 0;
	std::wstring syntax = syntax_;
	const int filter_plus = 1;
	const int filter_minus = 2;
	const int filter_normal = 3;
	const int filter_compat = 3;

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_TRUE(_T("unique"), unique)
			MAP_OPTIONS_BOOL_TRUE(_T("descriptions"), bShowDescriptions)
			MAP_OPTIONS_PUSH(_T("file"), files)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_BOOL_EX(_T("filter"), bFilterNew, _T("new"), _T("old"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), bFilterAll, _T("all"), _T("any"))
			MAP_OPTIONS_STR(_T("syntax"), syntax)
			/*
			MAP_FILTER_OLD("filter-eventType", eventType)
			MAP_FILTER_OLD("filter-severity", eventSeverity)
			MAP_FILTER_OLD("filter-eventID", eventID)
			MAP_FILTER_OLD("filter-eventSource", eventSource)
			MAP_FILTER_OLD("filter-generated", timeGenerated)
			MAP_FILTER_OLD("filter-written", timeWritten)
			MAP_FILTER_OLD("filter-message", message)
*/
			MAP_FILTER(_T("filter+eventType"), eventType, filter_plus)
			MAP_FILTER(_T("filter+severity"), eventSeverity, filter_plus)
			MAP_FILTER(_T("filter+eventID"), eventID, filter_plus)
			MAP_FILTER(_T("filter+eventSource"), eventSource, filter_plus)
			MAP_FILTER(_T("filter+generated"), timeGenerated, filter_plus)
			MAP_FILTER(_T("filter+written"), timeWritten, filter_plus)
			MAP_FILTER(_T("filter+message"), message, filter_plus)

			MAP_FILTER(_T("filter.eventType"), eventType, filter_normal)
			MAP_FILTER(_T("filter.severity"), eventSeverity, filter_normal)
			MAP_FILTER(_T("filter.eventID"), eventID, filter_normal)
			MAP_FILTER(_T("filter.eventSource"), eventSource, filter_normal)
			MAP_FILTER(_T("filter.generated"), timeGenerated, filter_normal)
			MAP_FILTER(_T("filter.written"), timeWritten, filter_normal)
			MAP_FILTER(_T("filter.message"), message, filter_normal)

			MAP_FILTER(_T("filter-eventType"), eventType, filter_minus)
			MAP_FILTER(_T("filter-severity"), eventSeverity, filter_minus)
			MAP_FILTER(_T("filter-eventID"), eventID, filter_minus)
			MAP_FILTER(_T("filter-eventSource"), eventSource, filter_minus)
			MAP_FILTER(_T("filter-generated"), timeGenerated, filter_minus)
			MAP_FILTER(_T("filter-written"), timeWritten, filter_minus)
			MAP_FILTER(_T("filter-message"), message, filter_minus)

			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	}

	unsigned long int hit_count = 0;

	for (std::list<std::wstring>::const_iterator cit2 = files.begin(); cit2 != files.end(); ++cit2) {
		HANDLE hLog = OpenEventLog(NULL, (*cit2).c_str());
		if (hLog == NULL) {
			message = _T("Could not open the '") + (*cit2) + _T("' event log: ") + error::lookup::last_error();
			return NSCAPI::returnUNKNOWN;
		}
		uniq_eventlog_map uniq_records;

		//DWORD dwThisRecord;
		DWORD dwRead, dwNeeded;
		EVENTLOGRECORD *pevlr;
		BYTE bBuffer[BUFFER_SIZE]; 

		pevlr = reinterpret_cast<EVENTLOGRECORD*>(&bBuffer);

		__time64_t ltime;
		_time64(&ltime);

		//GetOldestEventLogRecord(hLog, &dwThisRecord);

		while (ReadEventLog(hLog, EVENTLOG_FORWARDS_READ|EVENTLOG_SEQUENTIAL_READ,
			0, pevlr, BUFFER_SIZE, &dwRead, &dwNeeded))
		{
			while (dwRead > 0) 
			{ 
				//bool bMatch = bFilterAll;
				bool bMatch = !bFilterIn;
				EventLogRecord record((*cit2), pevlr, ltime);

				if (filter_chain.empty()) {
					message = _T("No filters specified.");
					return NSCAPI::returnUNKNOWN;
				}


				for (filterlist_type::const_iterator cit3 = filter_chain.begin(); cit3 != filter_chain.end(); ++cit3 ) {
					std::wstring reason;
					int mode = (*cit3).first;
					bool bTmpMatched = (*cit3).second.matchFilter(record);
					if (!bFilterNew) {
						if (bFilterAll) {
							if (!bTmpMatched) {
								bMatch = false;
								break;
							}
						} else {
							if (bTmpMatched) {
								bMatch = true;
								break;
							}
						}
					} else {
						if ((mode == filter_minus)&&(bTmpMatched)) {
							// a -<filter> hit so thrash item and bail out!
							if (debug_)
								NSC_DEBUG_MSG_STD(_T("Matched: - ") + (*cit3).second.getValue() + _T(" for: ") + record.render(bShowDescriptions, syntax));
							bMatch = false;
							break;
						} else if ((mode == filter_plus)&&(!bTmpMatched)) {
							// a +<filter> missed hit so thrash item and bail out!
							if (debug_)
								NSC_DEBUG_MSG_STD(_T("Matched: + ") + (*cit3).second.getValue() + _T(" for: ") + record.render(bShowDescriptions, syntax));
							bMatch = false;
							break;
						} else if (bTmpMatched) {
							if (debug_)
								NSC_DEBUG_MSG_STD(_T("Matched: . (contiunue): ") + (*cit3).second.getValue() + _T(" for: ") + record.render(bShowDescriptions, syntax));
							bMatch = true;
						}
					}
				}
				bool match = false;
				if ((!bFilterNew)&&((bFilterIn&&bMatch)||(!bFilterIn&&!bMatch))) {
					match = true;
				} else if (bFilterNew&&bMatch) {
					match = true;
				}
				if (match&&unique) {
					match = false;
					uniq_eventlog_record uniq_record = pevlr;
					uniq_eventlog_map::iterator it = uniq_records.find(uniq_record);
					if (it != uniq_records.end()) {
						(*it).second ++;
						//match = false;
					}
					else {
						if (!syntax.empty()) {
							uniq_record.message = record.render(bShowDescriptions, syntax);
						} else if (!bShowDescriptions) {
							uniq_record.message = record.eventSource();
						} else {
							uniq_record.message = record.eventSource();
							uniq_record.message += _T("(") + EventLogRecord::translateType(record.eventType()) + _T(", ") + 
								strEx::itos(record.eventID()) + _T(", ") + EventLogRecord::translateSeverity(record.severity()) + _T(")");
							uniq_record.message += _T("[") + record.enumStrings() + _T("]");
							uniq_record.message += _T("{%count%}");
						}
						uniq_records[uniq_record] = 1;
					}
					hit_count++;
				} else if (match) {
					if (!syntax.empty()) {
						strEx::append_list(message, record.render(bShowDescriptions, syntax));
					} else if (!bShowDescriptions) {
						strEx::append_list(message, record.eventSource());
					} else {
						strEx::append_list(message, record.eventSource());
						message += _T("(") + EventLogRecord::translateType(record.eventType()) + _T(", ") + 
							strEx::itos(record.eventID()) + _T(", ") + EventLogRecord::translateSeverity(record.severity()) + _T(")");
						message += _T("[") + record.enumStrings() + _T("]");
					}
					hit_count++;
				}
				dwRead -= pevlr->Length; 
				pevlr = (EVENTLOGRECORD *) ((LPBYTE) pevlr + pevlr->Length); 
			} 
			pevlr = (EVENTLOGRECORD *) &bBuffer; 
		} 
		CloseEventLog(hLog);
		for (uniq_eventlog_map::const_iterator cit = uniq_records.begin(); cit != uniq_records.end(); ++cit) {
			std::wstring msg = (*cit).first.message;
			strEx::replace(msg, _T("%count%"), strEx::itos((*cit).second));
			strEx::append_list(message, msg);
		}
	}

	if (!bPerfData)
		query.perfData = false;
	query.runCheck(hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("Eventlog check ok");
	NSC_DEBUG_MSG_STD(_T("Result: ") + message) ;
	return returnCode;
}


NSC_WRAPPERS_MAIN_DEF(gCheckEventLog);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckEventLog);
