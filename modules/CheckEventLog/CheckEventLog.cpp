// CheckEventLog.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "CheckEventLog.h"
#include <strEx.h>
#include <time.h>

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
public:
	EventLogRecord(EVENTLOGRECORD *pevlr) : pevlr_(pevlr) {
	}
	inline DWORD timeGenerated() const {
		return pevlr_->TimeGenerated;
	}
	inline DWORD timeWritten() const {
		return pevlr_->TimeWritten;
	}
	inline std::string eventSource() const {
		return reinterpret_cast<LPSTR>(reinterpret_cast<LPBYTE>(pevlr_) + sizeof(EVENTLOGRECORD));
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

};


struct searchQuery {
		struct searchQueryItem {

			typedef enum {
				eventType, 
				eventSource, 
				timeWritten,
				timeGenerated,
				message,
				none
			} queryType;

			typedef enum { out, in, undefined } filterType;

			filterType filter_;
			queryType queryType_;
			DWORD dwValue_;
			boost::regex regexp_;
			

			searchQueryItem() 
				: queryType_(none), dwValue_(0), filter_(out) 
			{}
			searchQueryItem(filterType filter, queryType type, std::string str) 
				: queryType_(type), dwValue_(0), filter_(filter)
			{
				switch (queryType_ ) {
				case eventType:
					dwValue_ = EventLogRecord::translateType(str);
					break;

				case timeGenerated:
					dwValue_ = strEx::stoui_as_time(str)/1000;
					break;

				case eventSource:
				case message:
					try {
						regexp_ = str;
					} catch (const boost::bad_expression e) {
						throw (std::string)"Invalid syntax in regular expression:" + str;
					}
					break;
				}
			}

			searchQueryItem& operator=(const searchQueryItem &other) {
				queryType_ = other.queryType_;
				dwValue_ = other.dwValue_;
				filter_ = other.filter_;
				try {
					regexp_ = other.regexp_;
				} catch (const boost::bad_expression e) {
					throw (std::string)"Invalid syntax in regular expression:" + other.toString();
				}
				return *this;
			}

			bool match(DWORD now, const EventLogRecord &record) const {
				switch (queryType_) {
				case eventType:
					return record.eventType() & dwValue_;

				case eventSource:
					if (regexp_.empty())
						return false;
					return boost::regex_match(record.eventSource(), regexp_);

				case timeWritten:
					return record.timeWritten() < (now-dwValue_);

				case timeGenerated:
					return record.timeGenerated() < (now-dwValue_);

				case message:
					if (regexp_.empty())
						return false;
					return boost::regex_match(record.enumStrings(), regexp_);

				default:
					return false;
				}
			}
			std::string queryType2String(queryType query) const {
				switch (queryType_) {
				case eventType:
					return "eventType";
				case eventSource:
					return "eventSource";
				case timeWritten:
					return "timeWritten";
				case timeGenerated:
					return "timeGenerated";
				case message:
					return "message";
				default:
					return "unknown";
				}

			}

			std::string toString() const {
				std::stringstream ss;
				ss << " Type: " << queryType2String(queryType_) << " = " << dwValue_ << ", '" << regexp_ << "'";
				return ss.str();
			}
		};



	unsigned int truncate;
	unsigned warning_count;
	unsigned critical_count;
	bool descriptions;
	std::list<searchQueryItem> queries;

	searchQuery() : truncate(0), descriptions(false), warning_count(0), critical_count(0) {}

	std::string toString() {
		std::string ret;
		for (std::list<searchQuery::searchQueryItem>::const_iterator cit = queries.begin(); cit != queries.end(); ++cit ) {
			ret += (*cit).toString();
		}
		return ret;
	}

};

// checkEventLog file=application truncate=1024 descriptions filter=[out|in]
//					warning-count=3 critical-count=10
// filter type = warning AND generated > 1d
//
// match (type, "warning") && match(generated, "1d")
//					filer-eventType=warning 
//					filer-eventSource=
//					filer-date=4d
//
// CheckEventLog
// request: CheckEventLog&<logfile>&<Query strings>
// Return: <return state>&<log entry 1> - <log entry 2>...
// <return state>	0 - No errors
//					1 - Unknown
//					2 - Errors

// ./nrpe-2.0/src/check_nrpe -H 192.168.167 -p 5666 -c checkEventLog -a file=system file=application filter-eventType=warning filter-generated=1d descriptions filter-eventSource=Cdrom filter-eventSource=NSClient warning-count=3 critical-count=7 filter=in truncate=512
//
// Examples:
// CheckEventLog&Application&1&<type>&<query>&huffa...
// CheckEventLog&Application&warn.require.eventType=warning&critical.require.eventType=error&truncate=1024&descriptions&all.exclude.eventSourceRegexp=^(Win|Msi|NSClient\+\+|Userenv|ASP\.NET|LoadPerf|Outlook|Application E|NSClient).*
#define BUFFER_SIZE 1024*64
NSCAPI::nagiosReturn CheckEventLog::handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	if (command != "CheckEventLog")
		return NSCAPI::returnIgnored;
	NSCAPI::nagiosReturn rCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	std::string ret;
	searchQuery query;
	std::list<std::string> files;
	searchQuery::searchQueryItem::filterType filter = searchQuery::searchQueryItem::out;

	for (std::list<std::string>::const_iterator it = args.begin(); it!=args.end(); ++it) {
		try {
			if ((*it) == "descriptions") {
				query.descriptions = true;
			} else {
				std::pair<std::string,std::string> p = strEx::split((*it), "=");
				if (p.first == "truncate") {
					query.truncate = strEx::stoi(p.second);
				} else if (p.first == "file") {
					files.push_back(p.second);
				} else if (p.first == "filter") {
					if (p.second == "in")
						filter = searchQuery::searchQueryItem::in;
					else
						filter = searchQuery::searchQueryItem::out;
				} else if (p.first == "warning-count") {
					query.warning_count = strEx::stoi(p.second);
				} else if (p.first == "critical-count") {
					query.critical_count = strEx::stoi(p.second);

				} else if (p.first == "filter-eventType") {
					query.queries.push_back(searchQuery::searchQueryItem(filter, searchQuery::searchQueryItem::eventType, p.second));
				} else if (p.first == "filter-eventSource") {
					query.queries.push_back(searchQuery::searchQueryItem(filter, searchQuery::searchQueryItem::eventSource, p.second));
				} else if (p.first == "filter-generated") {
					query.queries.push_back(searchQuery::searchQueryItem(filter, searchQuery::searchQueryItem::timeGenerated, p.second));
				} else if (p.first == "filter-written") {
					query.queries.push_back(searchQuery::searchQueryItem(filter, searchQuery::searchQueryItem::timeWritten, p.second));
				} else if (p.first == "filter-message") {
					query.queries.push_back(searchQuery::searchQueryItem(filter, searchQuery::searchQueryItem::message, p.second));
				}
			}
		} catch (std::string s) {
			if (message.empty())
				message += "UNKNOWN: ";
			else
				message += ", ";
			message += s;
		}
	}
	if (!message.empty()) {
		return NSCAPI::returnUNKNOWN;
	}

	unsigned int hit_count = 0;

	for (std::list<std::string>::const_iterator cit2 = files.begin(); cit2 != files.end(); ++cit2) {
		HANDLE hLog = OpenEventLog(NULL, (*cit2).c_str());
		if (hLog == NULL) {
			message = "Could not open the '" + (*cit2) + "' event log.";
			return NSCAPI::returnUNKNOWN;
		}

		DWORD dwThisRecord, dwRead, dwNeeded;
		EVENTLOGRECORD *pevlr;
		BYTE bBuffer[BUFFER_SIZE]; 

		pevlr = reinterpret_cast<EVENTLOGRECORD*>(&bBuffer);

		__time64_t ltime;
		_time64(&ltime);
		DWORD currentTime = ltime;

		GetOldestEventLogRecord(hLog, &dwThisRecord);

		while (ReadEventLog(hLog, EVENTLOG_FORWARDS_READ|EVENTLOG_SEQUENTIAL_READ,
			0, pevlr, BUFFER_SIZE, &dwRead, &dwNeeded))
		{
			while (dwRead > 0) 
			{ 
				bool match = false;
				bool undefined = true;
				searchQuery::searchQueryItem::filterType tFilter = searchQuery::searchQueryItem::out;
				EventLogRecord record(pevlr);

				for (std::list<searchQuery::searchQueryItem>::const_iterator cit3 = query.queries.begin(); cit3 != query.queries.end(); ++cit3 ) {
					if ((*cit3).match(currentTime, record)) {
						if ((*cit3).filter_ == searchQuery::searchQueryItem::in)
							match = true;
						else {
							match = false;
						}
					}
				}

				if (match) {
					if (!ret.empty())
						ret += ", ";
					ret += record.eventSource();
					if (query.descriptions) {
						ret += "(" + EventLogRecord::translateType(record.eventType()) + ")";
						ret += "[" + record.enumStrings() + "]";
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

	if ((query.critical_count > 0) && (hit_count > query.critical_count)) {
		ret = "CRITICAL: " + strEx::itos(hit_count) + " > critical: " + ret;
		rCode = NSCAPI::returnCRIT;
	} else if ((query.warning_count > 0) && (hit_count > query.warning_count)) {
		ret = "WARNING: " + strEx::itos(hit_count) + " > warning: " + ret;
		rCode = NSCAPI::returnWARN;
	} else {
		ret = "OK: " + strEx::itos(hit_count) + ": " + ret;
	}
	if (query.truncate != 0)
		ret = ret.substr(0, query.truncate);
	if ((query.truncate > 0) && (ret.length() > query.truncate))
		ret = ret.substr(0, query.truncate);
	message = ret;
	return rCode;
}


NSC_WRAPPERS_MAIN_DEF(gCheckEventLog);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckEventLog);
