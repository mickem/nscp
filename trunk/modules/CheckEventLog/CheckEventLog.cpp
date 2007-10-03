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


bool CheckEventLog::loadModule() {
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
public:
	EventLogRecord(EVENTLOGRECORD *pevlr, __int64 currentTime) : pevlr_(pevlr), currentTime_(currentTime) {
	}
	inline __int64 timeGenerated() const {
		return (currentTime_-pevlr_->TimeGenerated)*1000;
	}
	inline __int64 timeWritten() const {
		return (currentTime_-pevlr_->TimeWritten)*1000;
	}
	inline std::string eventSource() const {
		return reinterpret_cast<LPSTR>(reinterpret_cast<LPBYTE>(pevlr_) + sizeof(EVENTLOGRECORD));
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
	std::string userSID() const {
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
		return std::string(domain) + "\\" + std::string(user);
	}
	*/

	std::string enumStrings() const {
		std::string ret;
		LPSTR p = reinterpret_cast<LPSTR>(reinterpret_cast<LPBYTE>(pevlr_) + pevlr_->StringOffset);
		for (unsigned int i =0;i<pevlr_->NumStrings;i++) {
			std::string s = p;
			if (!s.empty())
				s += ", ";
			ret += s;
			p+= strlen(p)+1;
		}
		return ret;
	}

	static DWORD appendType(DWORD dwType, std::string sType) {
		return dwType | translateType(sType);
	}
	static DWORD subtractType(DWORD dwType, std::string sType) {
		return dwType & (!translateType(sType));
	}
	static DWORD translateType(std::string sType) {
		if (sType == "error")
			return EVENTLOG_ERROR_TYPE;
		if (sType == "warning")
			return EVENTLOG_WARNING_TYPE;
		if (sType == "info")
			return EVENTLOG_INFORMATION_TYPE;
		if (sType == "auditSuccess")
			return EVENTLOG_AUDIT_SUCCESS;
		if (sType == "auditFailure")
			return EVENTLOG_AUDIT_FAILURE;
		return strEx::stoi(sType);
	}
	static std::string translateType(DWORD dwType) {
		if (dwType == EVENTLOG_ERROR_TYPE)
			return "error";
		if (dwType == EVENTLOG_WARNING_TYPE)
			return "warning";
		if (dwType == EVENTLOG_INFORMATION_TYPE)
			return "info";
		if (dwType == EVENTLOG_AUDIT_SUCCESS)
			return "auditSuccess";
		if (dwType == EVENTLOG_AUDIT_FAILURE)
			return "auditFailure";
		return strEx::itos(dwType);
	}
	static DWORD translateSeverity(std::string sType) {
		if (sType == "success")
			return 0;
		if (sType == "informational")
			return 1;
		if (sType == "warning")
			return 2;
		if (sType == "error")
			return 3;
		return strEx::stoi(sType);
	}
	static std::string translateSeverity(DWORD dwType) {
		if (dwType == 0)
			return "success";
		if (dwType == 1)
			return "informational";
		if (dwType == 2)
			return "warning";
		if (dwType == 3)
			return "error";
		return strEx::itos(dwType);
	}
	std::string render(std::string syntax) {
		strEx::replace(syntax, "%source%", eventSource());
		strEx::replace(syntax, "%generated%", strEx::format_date(pevlr_->TimeGenerated, DATE_FORMAT));
		strEx::replace(syntax, "%written%", strEx::format_date(pevlr_->TimeWritten, DATE_FORMAT));
		strEx::replace(syntax, "%type%", translateType(eventType()));
		strEx::replace(syntax, "%severity%", translateSeverity(severity()));
		strEx::replace(syntax, "%strings%", enumStrings());
		strEx::replace(syntax, "%id%", strEx::itos(eventID()));
		return syntax;
	}
};


struct eventlog_filter {
	filters::filter_all_strings eventSource;
	filters::filter_all_numeric<unsigned int, filters::handlers::eventtype_handler> eventType;
	filters::filter_all_numeric<unsigned int, filters::handlers::eventseverity_handler> eventSeverity;
	filters::filter_all_strings message;
	filters::filter_all_times timeWritten;
	filters::filter_all_times timeGenerated;
	filters::filter_all_numeric<DWORD, filters::handlers::eventtype_handler> eventID;

	inline bool hasFilter() {
		return eventSource.hasFilter() || eventType.hasFilter() || eventID.hasFilter() || eventSeverity.hasFilter() || message.hasFilter() || 
			timeWritten.hasFilter() || timeGenerated.hasFilter();
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
NSCAPI::nagiosReturn CheckEventLog::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	if (command != "CheckEventLog")
		return NSCAPI::returnIgnored;
	typedef checkHolders::CheckConatiner<checkHolders::MaxMinBoundsULongInteger> EventLogQueryConatiner;
	typedef std::pair<int,eventlog_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::string> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	std::list<std::string> files;
	filterlist_type filter_chain;
	EventLogQueryConatiner query;

	bool bPerfData = true;
	bool bFilterIn = true;
	bool bFilterAll = false;
	bool bFilterNew = false;
	bool bShowDescriptions = false;
	unsigned int truncate = 0;
	std::string syntax;
	const int filter_plus = 1;
	const int filter_minus = 2;
	const int filter_normal = 3;
	const int filter_compat = 3;

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query, "")
			MAP_OPTIONS_STR2INT("truncate", truncate)
			MAP_OPTIONS_BOOL_TRUE("descriptions", bShowDescriptions)
			MAP_OPTIONS_PUSH("file", files)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_BOOL_EX("filter", bFilterNew, "new", "old")
			MAP_OPTIONS_BOOL_EX("filter", bFilterIn, "in", "out")
			MAP_OPTIONS_BOOL_EX("filter", bFilterAll, "all", "any")
			MAP_OPTIONS_STR("syntax", syntax)
/*
			MAP_FILTER_OLD("filter-eventType", eventType)
			MAP_FILTER_OLD("filter-severity", eventSeverity)
			MAP_FILTER_OLD("filter-eventID", eventID)
			MAP_FILTER_OLD("filter-eventSource", eventSource)
			MAP_FILTER_OLD("filter-generated", timeGenerated)
			MAP_FILTER_OLD("filter-written", timeWritten)
			MAP_FILTER_OLD("filter-message", message)
*/
			MAP_FILTER("filter+eventType", eventType, filter_plus)
			MAP_FILTER("filter+severity", eventSeverity, filter_plus)
			MAP_FILTER("filter+eventID", eventID, filter_plus)
			MAP_FILTER("filter+eventSource", eventSource, filter_plus)
			MAP_FILTER("filter+generated", timeGenerated, filter_plus)
			MAP_FILTER("filter+written", timeWritten, filter_plus)
			MAP_FILTER("filter+message", message, filter_plus)

			MAP_FILTER("filter.eventType", eventType, filter_normal)
			MAP_FILTER("filter.severity", eventSeverity, filter_normal)
			MAP_FILTER("filter.eventID", eventID, filter_normal)
			MAP_FILTER("filter.eventSource", eventSource, filter_normal)
			MAP_FILTER("filter.generated", timeGenerated, filter_normal)
			MAP_FILTER("filter.written", timeWritten, filter_normal)
			MAP_FILTER("filter.message", message, filter_normal)

			MAP_FILTER("filter-eventType", eventType, filter_minus)
			MAP_FILTER("filter-severity", eventSeverity, filter_minus)
			MAP_FILTER("filter-eventID", eventID, filter_minus)
			MAP_FILTER("filter-eventSource", eventSource, filter_minus)
			MAP_FILTER("filter-generated", timeGenerated, filter_minus)
			MAP_FILTER("filter-written", timeWritten, filter_minus)
			MAP_FILTER("filter-message", message, filter_minus)

			MAP_OPTIONS_MISSING(message, "Unknown argument: ")
		MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	}

	unsigned long int hit_count = 0;

	for (std::list<std::string>::const_iterator cit2 = files.begin(); cit2 != files.end(); ++cit2) {
		std::cout << "Opening: " << (*cit2) << std::endl;
		HANDLE hLog = OpenEventLog(NULL, (*cit2).c_str());
		if (hLog == NULL) {
			message = "Could not open the '" + (*cit2) + "' event log: " + error::lookup::last_error();
			return NSCAPI::returnUNKNOWN;
		}

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
				bool bMatch = bFilterAll;
				EventLogRecord record(pevlr, ltime);

				if (filter_chain.empty()) {
					message = "No filters specified.";
					return NSCAPI::returnUNKNOWN;
				}


				for (filterlist_type::const_iterator cit3 = filter_chain.begin(); cit3 != filter_chain.end(); ++cit3 ) {
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
							bMatch = false;
							break;
						} else if ((mode == filter_plus)&&(!bTmpMatched)) {
								// a +<filter> missed hit so thrash item and bail out!
								bMatch = false;
								break;
						} else if (bTmpMatched) {
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

				if (match) {
					if (!syntax.empty()) {
						strEx::append_list(message, record.render(syntax));
					} else if (!bShowDescriptions) {
						strEx::append_list(message, record.eventSource());
					} else {
						strEx::append_list(message, record.eventSource());
						message += "(" + EventLogRecord::translateType(record.eventType()) + ", " + 
							strEx::itos(record.eventID()) + ", " + EventLogRecord::translateSeverity(record.severity()) + ")";
						message += "[" + record.enumStrings() + "]";
					}
					hit_count++;
				}
				dwRead -= pevlr->Length; 
				pevlr = (EVENTLOGRECORD *) ((LPBYTE) pevlr + pevlr->Length); 
			} 
			pevlr = (EVENTLOGRECORD *) &bBuffer; 
		} 
		CloseEventLog(hLog);
	}

	if (!bPerfData)
		query.perfData = false;
	query.runCheck(hit_count, returnCode, message, perf);
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + "...";
	if (message.empty())
		message = "Eventlog check ok";
	return returnCode;
}


NSC_WRAPPERS_MAIN_DEF(gCheckEventLog);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckEventLog);
