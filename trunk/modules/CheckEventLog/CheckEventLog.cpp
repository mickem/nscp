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

std::string CheckEventLog::getModuleName() {
	return "Event log Checker.";
}
NSCModuleWrapper::module_version CheckEventLog::getModuleVersion() {
	NSCModuleWrapper::module_version version = {0, 0, 1 };
	return version;
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
	inline DWORD timeGenerated() {
		return pevlr_->TimeGenerated;
	}
	inline DWORD timeWritten() {
		return pevlr_->TimeWritten;
	}
	inline std::string eventSource() {
		return reinterpret_cast<LPSTR>(reinterpret_cast<LPBYTE>(pevlr_) + sizeof(EVENTLOGRECORD));
	}

	inline DWORD eventType() {
		return pevlr_->EventType;
	}

	std::string enumStrings() {
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
		return 0;
	}

};


struct searchQuery {
	struct searchQueryBundle {
		struct searchQueryItem {
			DWORD eventType_;
			std::string eventSource_;
			boost::regex eventSourceRegExp_;
			bool notSetValue_;
			DWORD writtenBeforeDelta_ ;
			DWORD writtenAfterDelta_ ;
			DWORD generatedBeforeDelta_;
			DWORD generatedAfterDelta_;
			boost::regex regexp_;

			searchQueryItem(bool notSetValue) 
				: eventType_(0), notSetValue_(notSetValue), 
				writtenBeforeDelta_(0), writtenAfterDelta_(0) ,
				generatedBeforeDelta_(0), generatedAfterDelta_(0) 
			{}
			searchQueryItem& operator=(const searchQueryItem &other) {
				eventType_ = other.eventType_;
				eventSource_ = other.eventSource_;
				notSetValue_ = other.notSetValue_;
				writtenBeforeDelta_ = other.writtenBeforeDelta_;
				writtenAfterDelta_ = other.writtenAfterDelta_;
				generatedBeforeDelta_ = other.generatedBeforeDelta_;
				generatedAfterDelta_ = other.generatedAfterDelta_;
				try {
					regexp_ = other.regexp_;
				} catch (const boost::bad_expression e) {
					throw (std::string)"Invalid syntax in regular expression:" + other.toString();
				}
				try {
					eventSourceRegExp_ = other.eventSourceRegExp_;
				} catch (const boost::bad_expression e) {
					throw (std::string)"Invalid syntax in event source regular expression:" + other.toString();
				}
				return *this;
			}

			inline bool matchDateWritten(DWORD now, DWORD written) const {
				if ((writtenAfterDelta_ == 0)&&(writtenBeforeDelta_ == 0))
					return notSetValue_;
				bool ret = true;
				if (writtenAfterDelta_ != 0) {
					if (writtenAfterDelta_+written <= now)
						ret = false;
				}
				if (writtenBeforeDelta_ != 0) {
					if (writtenBeforeDelta_+written > now)
						ret = false;
				}
				return ret;
			}
			inline bool matchDateGenerated(DWORD now, DWORD written) const {
				if ((generatedAfterDelta_ == 0)&&(generatedBeforeDelta_ == 0))
					return notSetValue_;
				bool ret = true;
				if (generatedAfterDelta_ != 0) {
					if (generatedAfterDelta_+written <= now)
						ret = false;
				}
				if (generatedBeforeDelta_ != 0) {
					if (generatedBeforeDelta_+written > now)
						ret = false;
				}
				return ret;
			}
			inline bool matchType(DWORD eventType) const {
				if (eventType_ == 0)
					return notSetValue_;
				return eventType_ & eventType;
			}
			inline bool matchSource(std::string eventSource) const {
				if ((eventSource_.empty())&&eventSourceRegExp_.empty())
					return notSetValue_;
				else if (eventSource_.empty())
					return boost::regex_match(eventSource, eventSourceRegExp_);
				else if (eventSourceRegExp_.empty())
					return eventSource_ == eventSource;
				return boost::regex_match(eventSource, eventSourceRegExp_) && (eventSource_ == eventSource);
			}
			inline bool matchRegexp(std::string msg) const {
				if (regexp_.empty())
					return notSetValue_;
				return boost::regex_match(msg, regexp_);
			}
			std::string toString() const {
				std::stringstream ss;
				ss << "    Regexp: " << regexp_ << std::endl;
				ss << "    Event type: " << eventType_ << std::endl;
				ss << "    Event source: " << eventSource_ << std::endl;
				ss << "    Event source Regexp: " << eventSourceRegExp_ << std::endl;
				ss << "    Written delta: " << writtenAfterDelta_ << " > " << writtenBeforeDelta_ << std::endl;
				ss << "    Generated delta: " << generatedAfterDelta_ << " > " << generatedBeforeDelta_ << std::endl;
				return ss.str();
			}
		};
		struct searchQueryItem require;
		struct searchQueryItem exclude;
		searchQueryBundle() : require(true), exclude(false) {}
		std::string toString() {
			return "  Required:\n" + require.toString()  + "\n  Exclude:\n" + exclude.toString();
		}
	};

	searchQueryBundle warn;
	searchQueryBundle critical;
	unsigned int truncate;
	bool descriptions;
	searchQuery() : truncate(0), descriptions(false) {}

	std::string toString() {
		return "Warn:\n" + warn.toString()  + "\nCritical:\n" + critical.toString();
	}

};

void addToQueryItem(searchQuery::searchQueryBundle::searchQueryItem &item, std::string arg) {
	std::pair<std::string,std::string> p = strEx::split(arg, "=");
	if (p.first == "eventType")
		item.eventType_ = EventLogRecord::appendType(item.eventType_, p.second);
	else if (p.first == "eventSource")
		item.eventSource_ = p.second;
	else if (p.first == "eventSourceRegexp") {
		try {
			std::string s = p.second;
			item.eventSourceRegExp_ = s;
		} catch (const boost::bad_expression e) {
			item.eventSourceRegExp_ = "";
			throw (std::string)"Invalid syntax in regular expression:" + p.second;
		}
	}
	else if (p.first == "generatedBeforeDelta")
		item.generatedBeforeDelta_ = strEx::stoi(p.second);
	else if (p.first == "generatedAfterDelta")
		item.generatedAfterDelta_ = strEx::stoi(p.second);
	else if (p.first == "writtenBeforeDelta")
		item.writtenBeforeDelta_ = strEx::stoi(p.second);
	else if (p.first == "writtenAfterDelta")
		item.writtenAfterDelta_ = strEx::stoi(p.second);
	else if (p.first == "regexp") {
		try {
			item.regexp_ = p.second;
		} catch (const boost::bad_expression e) {
			item.regexp_ = "";
			throw (std::string)"Invalid syntax in regular expression:" + p.second;
		}
	} else
		throw (std::string)"Invalid argument: " + p.first;
	
}
void addToQueryBundle(searchQuery::searchQueryBundle &bundle, std::string arg) {
	std::pair<std::string,std::string> p = strEx::split(arg, ".");
	if (p.first == "require")
		addToQueryItem(bundle.require, p.second);
	else if (p.first == "exclude")
		addToQueryItem(bundle.exclude, p.second);
	else
		throw (std::string)"Invalid require/exclude: " + p.first;
}
void addToQuery(searchQuery &q, std::string arg) {
	std::pair<std::string,std::string> p = strEx::split(arg, ".");
	if (p.first == "warn")
		addToQueryBundle(q.warn, p.second);
	else if (p.first == "critical")
		addToQueryBundle(q.critical, p.second);
	else if (p.first == "all") {
		addToQueryBundle(q.warn, p.second);
		addToQueryBundle(q.critical, p.second);
	} else {
		std::pair<std::string,std::string> p = strEx::split(arg, "=");
		if (p.first == "truncate")
			q.truncate = strEx::stoi(p.second);
		else if (p.first == "descriptions")
			q.descriptions = true;
		else
			throw (std::string)"Invalid argument: " + arg;
	}
}

void buildQury(searchQuery &query, std::list<std::string> args) {
	for (std::list<std::string>::const_iterator it = args.begin(); it!=args.end(); it++) {
		NSC_DEBUG_MSG_STD("Adding: " + *it);
		addToQuery(query, *it);
	}
}
// CheckEventLog
// request: CheckEventLog&<logfile>&<Query strings>
// Return: <return state>&<log entry 1> - <log entry 2>...
// <return state>	0 - No errors
//					1 - Unknown
//					2 - Errors
// Examples:
// CheckEventLog&Application&1&<type>&<query>&huffa...
// CheckEventLog&Application&warn.require.eventType=warning&critical.require.eventType=error&truncate=1024&descriptions&all.exclude.eventSourceRegexp=^(Win|Msi|NSClient\+\+|Userenv|ASP\.NET|LoadPerf|Outlook|Application E|NSClient).*
#define BUFFER_SIZE 1024*64

NSCAPI::nagiosReturn CheckEventLog::handleCommand(const std::string command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf) {
	if (command != "CheckEventLog")
		return NSCAPI::returnIgnored;
	NSCAPI::nagiosReturn rCode = NSCAPI::returnOK;
	std::list<std::string> args = arrayBuffer::arrayBuffer2list(argLen, char_args);
	if (args.size() < 2) {
		message = "Missing argument";
		return NSCAPI::returnCRIT;
	}

	std::list<std::string>::iterator it = args.begin();
	for (;it != args.end();it++) {
		NSC_DEBUG_MSG_STD("Arguments: " + (*it));
	}
	std::string ret;
	bool critical = false;
	searchQuery query;
	std::string logFile = args.front(); args.pop_front();
	try {
		buildQury(query, args);
	} catch (std::string s) {
		message = s;
		return NSCAPI::returnCRIT;
	}
	NSC_DEBUG_MSG_STD("Base query: " + query.toString());

	HANDLE hLog = OpenEventLog(NULL, logFile.c_str());
	if (hLog == NULL) {
		message = "Could not open the Application event log.";
		return NSCAPI::returnUNKNOWN;
	}

	DWORD dwThisRecord, dwRead, dwNeeded;
	EVENTLOGRECORD *pevlr;
	BYTE bBuffer[BUFFER_SIZE]; 

	pevlr = reinterpret_cast<EVENTLOGRECORD*>(&bBuffer);

	// get time now !!!
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
			bool c = false;
			EventLogRecord record(pevlr);

			if ( query.critical.require.matchType(record.eventType()) &&
				query.critical.require.matchSource(record.eventSource()) &&
				query.critical.require.matchDateGenerated(currentTime, record.timeGenerated()) &&
				query.critical.require.matchDateWritten(currentTime, record.timeWritten()) &&
				query.critical.require.matchRegexp(record.enumStrings())
				) {
					match = true;
					c = true;
				}
			if ( query.critical.exclude.matchType(record.eventType()) ||
				query.critical.exclude.matchSource(record.eventSource()) ||
				query.critical.exclude.matchDateGenerated(currentTime, record.timeGenerated()) ||
				query.critical.exclude.matchDateWritten(currentTime, record.timeWritten()) ||
				query.critical.exclude.matchRegexp(record.enumStrings())
				) {
					match = false;
					c = false;
				}

			if ( query.warn.require.matchType(record.eventType()) &&
				query.warn.require.matchSource(record.eventSource()) &&
				query.warn.require.matchDateGenerated(currentTime, record.timeGenerated()) &&
				query.warn.require.matchDateWritten(currentTime, record.timeWritten()) &&
				query.warn.require.matchRegexp(record.enumStrings())
				)
				match = true;
			if ( query.warn.exclude.matchType(record.eventType()) ||
				query.warn.exclude.matchSource(record.eventSource()) ||
				query.warn.exclude.matchDateGenerated(currentTime, record.timeGenerated()) ||
				query.warn.exclude.matchDateWritten(currentTime, record.timeWritten()) ||
				query.warn.exclude.matchRegexp(record.enumStrings())
				)
				match = false;
			
			if (match) {
				if (c)
					critical = true;
				if (!ret.empty())
					ret += " - ";
				ret += record.eventSource();
				if (query.descriptions) {
					std::string s = record.enumStrings();
					if (!s.empty())
						ret += " [" + s + "]" ;
				}
			}
			dwRead -= pevlr->Length; 
			pevlr = (EVENTLOGRECORD *) 
				((LPBYTE) pevlr + pevlr->Length); 
		} 

		pevlr = (EVENTLOGRECORD *) &bBuffer; 
	} 

	CloseEventLog(hLog);
	if (critical) {
		ret = "CRITICAL: " + ret;
		rCode = NSCAPI::returnCRIT;
	}
	else if (!ret.empty()) {
		ret = "WARNING: " + ret;
		rCode = NSCAPI::returnWARN;
	} else 
		ret = "OK: No errors/warnings in event log.";
	if (query.truncate != 0)
		ret = ret.substr(0, query.truncate);
	message = ret;
	return rCode;
}


NSC_WRAPPERS_MAIN_DEF(gCheckEventLog);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckEventLog);
