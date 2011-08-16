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
#include <boost/foreach.hpp>

#include <strEx.h>
#include <time.h>
#include <utils.h>
#include <error.hpp>
#include <map>
#include <vector>
#include <config.h>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include "filter.hpp"


#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <simple_timer.hpp>
#include <settings/client/settings_client.hpp>
namespace sh = nscapi::settings_helper;

#include "simple_registry.hpp"
#include "eventlog_record.hpp"

CheckEventLog gCheckEventLog;

CheckEventLog::CheckEventLog() {
}
CheckEventLog::~CheckEventLog() {
}
struct parse_exception {
	parse_exception(std::wstring) {}
};




bool CheckEventLog::loadModule() {
	return false;
}


bool CheckEventLog::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		get_core()->registerCommand(_T("CheckEventLog"), _T("Check for errors in the event logger!"));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("CheckEventlog"), alias);

		settings.alias().add_path_to_settings()
			(_T("EVENT LOG SECTION"), _T("Section for the EventLog Checker (CHeckEventLog.dll)."))
			;

		settings.alias().add_key_to_settings()
			(_T("debug"), sh::bool_key(&debug_, false),
			_T("DEBUG"), _T("Log all \"hits\" and \"misses\" on the eventlog filter chain, useful for debugging eventlog checks but very very very noisy so you don't want to accidentally set this on a real machine."))

			(_T("lookup names"), sh::bool_key(&lookup_names_, false),
			_T("LOOKUP NAMES"), _T(""))

			(_T("syntax"), sh::wstring_key(&syntax_),
			_T("SYNTAX"), _T("Set this to use a specific syntax string for all commands (that don't specify one)."))

			(_T("buffer size"), sh::int_key(&buffer_length_, 128*1024),
			_T("BUFFER_SIZE"), _T("The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve."))
			;

		settings.register_all();
		settings.notify();

	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
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

std::wstring find_eventlog_name(std::wstring name) {
	try {
		simple_registry::registry_key key(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog"));
		std::list<std::wstring> list = key.get_keys();
		for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
			try {
				simple_registry::registry_key sub_key(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\") + *cit);
				std::wstring file = sub_key.get_string(_T("DisplayNameFile"));
				int id = sub_key.get_int(_T("DisplayNameID"));
				std::wstring real_name = error::format::message::from_module(file, id);
				strEx::replace(real_name, _T("\n"), _T(""));
				strEx::replace(real_name, _T("\r"), _T(""));
				NSC_DEBUG_MSG(_T("Attempting to match: ") + real_name + _T(" with ") + name);
				if (real_name == name)
					return *cit;
			} catch (simple_registry::registry_exception &e) { e;}
		}
		return name;
	} catch (simple_registry::registry_exception &e) {
		NSC_DEBUG_MSG(_T("Failed to get eventlog name (assuming shorthand): ") + e.what());
		return name;
	} catch (...) {
		NSC_DEBUG_MSG(_T("Failed to get eventlog name (assuming shorthand)"));
		return name;
	}
}



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

struct event_log_buffer {
	BYTE *bBuffer;
	DWORD bufferSize_;
	event_log_buffer(DWORD bufferSize) : bufferSize_(bufferSize) {
		bBuffer = new BYTE[bufferSize+10];
	}
	~event_log_buffer() {
		delete [] bBuffer;
	}
	EVENTLOGRECORD* getBufferUnsafe() {
		return reinterpret_cast<EVENTLOGRECORD*>(bBuffer);
	}
	DWORD getBufferSize() {
		return bufferSize_;
	}
};

NSCAPI::nagiosReturn CheckEventLog::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command != _T("checkeventlog"))
		return NSCAPI::returnIgnored;
	simple_timer time;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsULongInteger> EventLogQuery1Container;
	typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULongInteger> EventLogQuery2Container;
	
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;

	std::list<std::wstring> files;
	EventLogQuery1Container query1;
	EventLogQuery2Container query2;


	eventlog_filter::filter_argument fargs = eventlog_filter::factories::create_argument(syntax_, DATE_FORMAT);

	bool bPerfData = true;
	bool unique = false;
	unsigned int truncate = 0;
	event_log_buffer buffer(buffer_length_);
	//bool bPush = true;

	try {
		MAP_OPTIONS_BEGIN(arguments)
			MAP_OPTIONS_NUMERIC_ALL(query1, _T(""))
			MAP_OPTIONS_EXACT_NUMERIC_ALL(query2, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_TRUE(_T("unique"), unique)
			MAP_OPTIONS_BOOL_TRUE(_T("descriptions"), fargs->bShowDescriptions)
			MAP_OPTIONS_PUSH(_T("file"), files)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_BOOL_EX(_T("filter"), fargs->bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), fargs->bFilterAll, _T("all"), _T("any"))
			MAP_OPTIONS_BOOL_EX(_T("debug"), fargs->debug, _T("true"), _T("false"))
			MAP_OPTIONS_STR(_T("syntax"), fargs->syntax)
			MAP_OPTIONS_STR(_T("filter"), fargs->filter)
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
		} catch (checkHolders::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = _T("Invalid command line!");
		return NSCAPI::returnUNKNOWN;
	}

	unsigned long int hit_count = 0;
	if (files.empty()) {
		message = _T("No file specified try adding: file=Application");
		return NSCAPI::returnUNKNOWN;
	}
	bool buffer_error_reported = false;


	eventlog_filter::filter_engine impl = eventlog_filter::factories::create_engine(fargs);

	if (!impl) {
		message = _T("Failed to initialize filter subsystem.");
		return NSCAPI::returnUNKNOWN;
	}

	impl->boot();

	__time64_t ltime;
	_time64(&ltime);

	if (!impl->validate(message)) {
		return NSCAPI::returnUNKNOWN;
	}


	NSC_DEBUG_MSG_STD(_T("Boot time: ") + strEx::itos(time.stop()));

	for (std::list<std::wstring>::const_iterator cit2 = files.begin(); cit2 != files.end(); ++cit2) {
		std::wstring name = *cit2;
		if (lookup_names_) {
			name = find_eventlog_name(*cit2);
			if ((*cit2) != name) {
				NSC_DEBUG_MSG_STD(_T("Opening alternative log: ") + name);
			}
		}
		HANDLE hLog = OpenEventLog(NULL, name.c_str());
		if (hLog == NULL) {
			message = _T("Could not open the '") + (*cit2) + _T("' event log: ") + error::lookup::last_error();
			return NSCAPI::returnUNKNOWN;
		}
		uniq_eventlog_map uniq_records;

		DWORD dwRead, dwNeeded;
		while (true) {
			BOOL bStatus = ReadEventLog(hLog, EVENTLOG_FORWARDS_READ|EVENTLOG_SEQUENTIAL_READ,
				0, buffer.getBufferUnsafe(), buffer.getBufferSize(), &dwRead, &dwNeeded);
			if (bStatus == FALSE) {
				DWORD err = GetLastError();
				if (err == ERROR_INSUFFICIENT_BUFFER) {
					if (!buffer_error_reported) {
						NSC_LOG_ERROR_STD(_T("EvenlogBuffer is too small change the value of buffer_length=") + strEx::itos(dwNeeded+1) + _T(": ") + error::lookup::last_error(err));
						buffer_error_reported = true;
					}
				} else if (err == ERROR_HANDLE_EOF) {
					break;
				} else {
					NSC_LOG_ERROR_STD(_T("Failed to read from eventlog: ") + error::lookup::last_error(err));
					message = _T("Failed to read from eventlog: ") + error::lookup::last_error(err);
					CloseEventLog(hLog);
					return NSCAPI::returnUNKNOWN;
				}
			}
			EVENTLOGRECORD *pevlr = buffer.getBufferUnsafe(); 
			while (dwRead > 0) { 
				EventLogRecord record((*cit2), pevlr, ltime);
				boost::shared_ptr<eventlog_filter::filter_obj> arg = boost::shared_ptr<eventlog_filter::filter_obj>(new eventlog_filter::filter_obj(record));
				bool match = impl->match(arg);
				if (match&&unique) {
					match = false;
					uniq_eventlog_record uniq_record = pevlr;
					uniq_eventlog_map::iterator it = uniq_records.find(uniq_record);
					if (it != uniq_records.end()) {
						(*it).second ++;
					}
					else {
						if (!fargs->syntax.empty()) {
							uniq_record.message = record.render(fargs->bShowDescriptions, fargs->syntax);
						} else if (!fargs->bShowDescriptions) {
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
					if (!fargs->syntax.empty()) {
						strEx::append_list(message, record.render(fargs->bShowDescriptions, fargs->syntax));
					} else if (!fargs->bShowDescriptions) {
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
				pevlr = reinterpret_cast<EVENTLOGRECORD*>((LPBYTE)pevlr + pevlr->Length); 
			} 
		}
		CloseEventLog(hLog);
		for (uniq_eventlog_map::const_iterator cit = uniq_records.begin(); cit != uniq_records.end(); ++cit) {
			std::wstring msg = (*cit).first.message;
			strEx::replace(msg, _T("%count%"), strEx::itos((*cit).second));
			strEx::append_list(message, msg);
		}
	}
	NSC_DEBUG_MSG_STD(_T("Evaluation time: ") + strEx::itos(time.stop()));

	if (!bPerfData) {
		query1.perfData = false;
		query2.perfData = false;
	}
	if (query1.alias.empty())
		query1.alias = _T("eventlog");
	if (query2.alias.empty())
		query2.alias = _T("eventlog");
	if (query1.hasBounds())
		query1.runCheck(hit_count, returnCode, message, perf);
	else if (query2.hasBounds())
		query2.runCheck(hit_count, returnCode, message, perf);
	else {
		message = _T("No bounds specified!");
		return NSCAPI::returnUNKNOWN;
	}
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("Eventlog check ok");
	return returnCode;
}


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gCheckEventLog);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckEventLog);
