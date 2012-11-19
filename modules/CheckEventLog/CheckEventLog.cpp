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

#include <time.h>
#include <utils.h>
#include <error.hpp>
#include <map>
#include <vector>
//#include <config.h>

#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>

#include "filter.hpp"
#include "filters.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

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


void real_time_thread::process_no_events(const filters::filter_config_object &object) {
	std::wstring response;
	std::wstring command = object.alias;
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, NSCAPI::returnOK, object.ok_msg, object.perf_msg, response)) {
		NSC_LOG_ERROR(_T("Failed to submit evenhtlog result: ") + response);
	}
}

void real_time_thread::process_record(const filters::filter_config_object &object, const EventLogRecord &record) {
	std::wstring response;
	int severity = object.severity;
	std::wstring command = object.alias;
	if (severity == -1) {
		NSC_LOG_ERROR(_T("Severity not defined for: ") + object.alias);
		severity = NSCAPI::returnUNKNOWN;
	}
	if (!object.command.empty())
		command = object.command;
	std::wstring message = record.render(true, object.syntax, object.date_format, object.dwLang);
	if (!nscapi::core_helper::submit_simple_message(object.target, command, object.severity, message, object.perf_msg, response)) {
		NSC_LOG_ERROR(_T("Failed to submit eventlog result ") + object.alias + _T(": ") + response);
	}
}

void real_time_thread::debug_miss(const EventLogRecord &record) {
	std::wstring message = record.render(true, _T("%id% %level% %source%: %message%"), DATE_FORMAT, LANG_NEUTRAL);
	NSC_DEBUG_MSG_STD(_T("No filter matched: ") + message);
}

void real_time_thread::thread_proc() {

	std::list<filters::filter_config_object> filters;
	std::list<std::wstring> logs;
	std::list<std::wstring> filter_list;

	BOOST_FOREACH(std::wstring s, strEx::splitEx(logs_, _T(","))) {
		logs.push_back(s);
	}

	BOOST_FOREACH(filters::filter_config_object object, filters_.get_object_list()) {
		eventlog_filter::filter_argument fargs = eventlog_filter::factories::create_argument(object.syntax, object.date_format);
		fargs->filter = object.filter;
		fargs->debug = object.debug;
		fargs->alias = object.alias;
		fargs->bShowDescriptions = true;
		if (object.log_ != _T("any") && object.log_ != _T("all"))
			logs.push_back(object.log_);
		// eventlog_filter::filter_engine 
		object.engine = eventlog_filter::factories::create_engine(fargs);

		if (!object.engine) {
			NSC_LOG_ERROR_STD(_T("Invalid filter: ") + object.filter);
			continue;
		}

		if (!object.engine->boot()) {
			NSC_LOG_ERROR_STD(_T("Error booting filter: ") + object.filter);
			continue;
		}

		std::wstring message;
		if (!object.engine->validate(message)) {
			NSC_LOG_ERROR_STD(_T("Error validating filter: ") + message);
			continue;
		}
		filters.push_back(object);
		filter_list.push_back(object.alias);
	}
	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD(_T("Scanning logs: ") + strEx::joinEx(logs, _T(", ")));
	NSC_DEBUG_MSG_STD(_T("Scanning filters: ") + strEx::joinEx(filter_list, _T(", ")));

	typedef boost::shared_ptr<eventlog_wrapper> eventlog_type;
	typedef std::vector<eventlog_type> eventlog_list;
	eventlog_list evlog_list;

	BOOST_FOREACH(std::wstring l, logs) {
		eventlog_type el = eventlog_type(new eventlog_wrapper(l));
		if (!el->seek_end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find the end of eventlog: ") + l);
		} else {
			evlog_list.push_back(el);
		}
	}
	
	// TODO: add support for scanning "missed messages" at startup

	HANDLE *handles = new HANDLE[1+evlog_list.size()];
	handles[0] = stop_event_;
	for (int i=0;i<evlog_list.size();i++) {
		evlog_list[i]->notify(handles[i+1]);
	}
	__time64_t ltime;
	_time64(&ltime);

	BOOST_FOREACH(filters::filter_config_object &object, filters) {
		object.touch(ltime);
	}

	unsigned int errors = 0;
	while (true) {

		DWORD minNext = INFINITE;
		BOOST_FOREACH(const filters::filter_config_object &object, filters) {
			NSC_DEBUG_MSG_STD(_T("Getting next from: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			if (object.next_ok_ > 0 && object.next_ok_ < minNext)
				minNext = object.next_ok_;
		}

		_time64(&ltime);

		if (ltime > minNext) {
			NSC_LOG_ERROR(_T("Strange seems we expect to send ok now?"));
			continue;
		}
		DWORD dwWaitTime = (minNext - ltime)*1000;
		if (minNext == INFINITE || dwWaitTime < 0)
			dwWaitTime = INFINITE;
		NSC_DEBUG_MSG(_T("Next miss time is in: ") + strEx::itos(dwWaitTime) + _T("s"));

		DWORD dwWaitReason = WaitForMultipleObjects(evlog_list.size()+1, handles, FALSE, dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			// we take care of this below...
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete [] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + evlog_list.size())) {

			eventlog_type el = evlog_list[dwWaitReason-WAIT_OBJECT_0-1];
			DWORD status = el->read_record(0, EVENTLOG_SEQUENTIAL_READ|EVENTLOG_FORWARDS_READ);
			if (ERROR_SUCCESS != status && ERROR_HANDLE_EOF != status) {
				delete [] handles;
				return;
			}

			_time64(&ltime);

			EVENTLOGRECORD *pevlr = el->read_record_with_buffer();
			while (pevlr != NULL) {
				EventLogRecord elr(el->get_name(), pevlr, ltime);
				boost::shared_ptr<eventlog_filter::filter_obj> arg = boost::shared_ptr<eventlog_filter::filter_obj>(new eventlog_filter::filter_obj(elr));
				bool matched = false;

				BOOST_FOREACH(filters::filter_config_object &object, filters) {
					if (object.log_ != _T("any") && object.log_ != _T("all") && object.log_ != el->get_name()) {
						NSC_DEBUG_MSG_STD(_T("Skipping filter: ") + object.alias);
						continue;
					}
					if (object.engine->match(arg)) {
						process_record(object, elr);
						object.touch(ltime);
						matched = true;
					}
				}
				if (debug_ && !matched)
					debug_miss(elr);

				pevlr = el->read_record_with_buffer();
			}
		} else {
			NSC_LOG_ERROR(_T("Error failed to wait for eventlog message: ") + error::lookup::last_error());
			if (errors++ > 10) {
				NSC_LOG_ERROR(_T("To many errors giving up"));
				delete [] handles;
				return;
			}
		}
		_time64(&ltime);
		BOOST_FOREACH(filters::filter_config_object &object, filters) {
			if (object.next_ok_ != 0 && object.next_ok_ <= (ltime+1)) {
				process_no_events(object);
				object.touch(ltime);
			} else {
				NSC_DEBUG_MSG_STD(_T("missing: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			}
		}
	}
	delete [] handles;
	return;
}


bool real_time_thread::start() {
	if (!enabled_)
		return true;

	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("EventLogShutdown"));

	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&real_time_thread::thread_proc, this)));
	return true;
}
bool real_time_thread::stop() {
	SetEvent(stop_event_);
	if (thread_)
		thread_->join();
	return true;
}

void real_time_thread::add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring key, std::wstring query) {
	try {
		filters_.add(proxy, filters_path_, key, query, key == _T("default"));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key);
	}
}


bool CheckEventLog::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		register_command(_T("CheckEventLog"), _T("Check for errors in the event logger!"));
		register_command(_T("check_eventlog"), _T("Check for errors in the event logger!"));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, _T("eventlog"));
		
		thread_.filters_path_ = settings.alias().get_settings_path(_T("real-time/filters"));


		settings.alias().add_path_to_settings()
			(_T("EVENT LOG SECTION"), _T("Section for the EventLog Checker (CheckEventLog.dll)."))

			(_T("real-time"), _T("CONFIGURE REALTIME CHECKING"), _T("A set of options to configure the real time checks"))

			(_T("real-time/filters"), sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, &thread_, get_settings_proxy(), _1, _2)),  
			_T("REALTIME FILTERS"), _T("A set of filters to use in real-time mode"))
			;

		settings.alias().add_key_to_settings()
			(_T("debug"), sh::bool_key(&debug_, false),
			_T("DEBUG"), _T("Log more information when filtering (usefull to detect issues with filters) not usefull in production as it is a bit of a resource hog."))

			(_T("lookup names"), sh::bool_key(&lookup_names_, true),
			_T("LOOKUP NAMES"), _T("Lookup the names of eventlog files"))

			(_T("syntax"), sh::wstring_key(&syntax_),
			_T("SYNTAX"), _T("Set this to use a specific syntax string for all commands (that don't specify one)."))

			(_T("buffer size"), sh::int_key(&buffer_length_, 128*1024),
			_T("BUFFER_SIZE"), _T("The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve."))

			;

		settings.alias().add_key_to_settings(_T("real-time"))

			(_T("enabled"), sh::bool_fun_key<bool>(boost::bind(&real_time_thread::set_enabled, &thread_, _1), false),
			_T("REAL TIME CHECKING"), _T("Spawns a backgrounnd thread which detects issues and reports them back instantly."))

			(_T("startup age"), sh::string_fun_key<std::wstring>(boost::bind(&real_time_thread::set_start_age, &thread_, _1), _T("30m")),
			_T("STARTUP AGE"), _T("The initial age to scan when starting NSClient++"))

			(_T("log"), sh::wstring_key(&thread_.logs_ ,_T("application,system")),
			_T("LOGS TO CHECK"), _T("Comma separated list of logs to check"))

			(_T("debug"), sh::bool_key(&thread_.debug_, false),
			_T("DEBUG"), _T("Log missed records (usefull to detect issues with filters) not usefull in production as it is a bit of a resource hog."))

			;

		settings.register_all();
		settings.notify();

		if (mode == NSCAPI::normalStart) {
			if (!thread_.start())
				NSC_LOG_ERROR_STD(_T("Failed to start collection thread"));
		}

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	return true;
}
bool CheckEventLog::unloadModule() {
	if (!thread_.stop())
		NSC_LOG_ERROR_STD(_T("Failed to start collection thread"));
	return true;
}

bool CheckEventLog::hasCommandHandler() {
	return true;
}
bool CheckEventLog::hasMessageHandler() {
	return false;
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
typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsULongInteger> EventLogQuery1Container;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULongInteger> EventLogQuery2Container;
NSCAPI::nagiosReturn CheckEventLog::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command != _T("checkeventlog") && command != _T("check_eventlog"))
		return NSCAPI::returnIgnored;
	simple_timer time;
	
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
			name = eventlog_wrapper::find_eventlog_name(*cit2);
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
							uniq_record.message = record.get_source();
						} else {
							uniq_record.message = record.get_source();
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
						strEx::append_list(message, record.get_source());
					} else {
						strEx::append_list(message, record.get_source());
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
NSCAPI::nagiosReturn CheckEventLog::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response) {
	std::wstring command = char_command;
	if (command == _T("insert-eventlog-message") || command == _T("insert-eventlog") || command == _T("insert-message") || command == _T("insert") || command.empty()) {
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_exec_request(char_command, request);
		std::wstring message;
		std::vector<std::wstring> args(data.args.begin(), data.args.end());
		bool ok = insert_eventlog(args, message);
		nscapi::functions::create_simple_exec_response(command, ok?NSCAPI::isSuccess:NSCAPI::hasFailed, message, response);
		return ok?NSCAPI::isSuccess:NSCAPI::hasFailed;
	} else if (command == _T("help")) {
		std::vector<std::wstring> args;
		args.push_back(_T("--help"));
		std::wstring message;
		insert_eventlog(args, message);
		nscapi::functions::create_simple_exec_response(command, NSCAPI::isSuccess, message, response);
		return NSCAPI::isSuccess;
	}
	return NSCAPI::returnIgnored;
}



NSCAPI::nagiosReturn CheckEventLog::insert_eventlog(std::vector<std::wstring> arguments, std::wstring &message) {
	try {
		namespace po = boost::program_options;

		bool help = false;
		std::wstring type, severity, source_name;
		std::vector<std::wstring> strings;
		WORD wEventID = 0, category = 0, customer = 0;
		WORD facility = 0;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", po::bool_switch(&help), "Show help screen")
			("source,s", po::wvalue<std::wstring>(&source_name)->default_value(_T("Application Error")), "source to use")
			("type,t", po::wvalue<std::wstring>(&type), "Event type")
			("level,l", po::wvalue<std::wstring>(&type), "Event level (type)")
			("facility,f", po::value<WORD>(&facility), "Facility/Qualifier")
			("qualifier,q", po::value<WORD>(&facility), "Facility/Qualifier")
			("severity", po::wvalue<std::wstring>(&severity), "Event severity")
			("category,c", po::value<WORD>(&category), "Event category")
			("customer", po::value<WORD>(&customer), "Customer bit 0,1")
			("arguments,a", po::wvalue<std::vector<std::wstring> >(&strings), "Message arguments (strings)")
			("eventlog-arguments", po::wvalue<std::vector<std::wstring> >(&strings), "Message arguments (strings)")
			("event-arguments", po::wvalue<std::vector<std::wstring> >(&strings), "Message arguments (strings)")
			("id,i", po::value<WORD>(&wEventID), "Event ID")
			;

		boost::program_options::variables_map vm;

		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(arguments).options(desc).run();
		po::store(parsed, vm);
		po::notify(vm);

		if (help || arguments.empty()) {
			std::stringstream ss;
			ss << "CheckEventLog Command line syntax:" << std::endl;
			ss << desc;
			message = utf8::cvt<std::wstring>(ss.str());
			return NSCAPI::isSuccess;
		} else {
			event_source source(source_name);
			WORD wType = EventLogRecord::translateType(type);
			WORD wSeverity = EventLogRecord::translateSeverity(severity);
			DWORD tID = (wEventID&0xffff) | ((facility&0xfff)<<16) | ((customer&0x1)<<29) | ((wSeverity&0x3)<<30);

			int size = 0;
			BOOST_FOREACH(const std::wstring &s, strings) {
				size += s.size()+1;
			}
			LPCWSTR *string_data = new LPCWSTR[strings.size()];
			int i=0;
			BOOST_FOREACH(const std::wstring &s, strings) {
				string_data[i++] = s.c_str();
			}

			if (!ReportEvent(source, wType, category, tID, NULL, strings.size(), 0, string_data, NULL)) {
				message = _T("Could not report the event"); 
				return NSCAPI::hasFailed;
			} else {
				message = _T("Message reported successfully"); 
			}
			delete [] string_data;
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to parse command line: ") + utf8::cvt<std::wstring>(e.what()));
	}
	return NSCAPI::returnIgnored;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(CheckEventLog, _T("eventlog"));
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
