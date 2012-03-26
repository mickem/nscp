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

WORD get_language(std::string lang) {
	if (lang == "neutral") return LANG_NEUTRAL;
	if (lang == "arabic") return LANG_ARABIC;
	if (lang == "bulgarian") return LANG_BULGARIAN;
	if (lang == "catalan") return LANG_CATALAN;
	if (lang == "chinese") return LANG_CHINESE;
	if (lang == "czech") return LANG_CZECH;
	if (lang == "danish") return LANG_DANISH;
	if (lang == "german") return LANG_GERMAN;
	if (lang == "greek") return LANG_GREEK;
	if (lang == "english") return LANG_ENGLISH;
	if (lang == "spanish") return LANG_SPANISH;
	if (lang == "finnish") return LANG_FINNISH;
	if (lang == "french") return LANG_FRENCH;
	if (lang == "hebrew") return LANG_HEBREW;
	if (lang == "hungarian") return LANG_HUNGARIAN;
	if (lang == "icelandic") return LANG_ICELANDIC;
	if (lang == "italian") return LANG_ITALIAN;
	if (lang == "japanese") return LANG_JAPANESE;
	if (lang == "korean") return LANG_KOREAN;
	if (lang == "dutch") return LANG_DUTCH;
	if (lang == "norwegian") return LANG_NORWEGIAN;
	if (lang == "polish") return LANG_POLISH;
	if (lang == "portuguese") return LANG_PORTUGUESE;
	if (lang == "romanian") return LANG_ROMANIAN;
	if (lang == "russian") return LANG_RUSSIAN;
	if (lang == "croatian") return LANG_CROATIAN;
	if (lang == "serbian") return LANG_SERBIAN;
	if (lang == "slovak") return LANG_SLOVAK;
	if (lang == "albanian") return LANG_ALBANIAN;
	if (lang == "swedish") return LANG_SWEDISH;
	if (lang == "thai") return LANG_THAI;
	if (lang == "turkish") return LANG_TURKISH;
	if (lang == "urdu") return LANG_URDU;
	if (lang == "indonesian") return LANG_INDONESIAN;
	if (lang == "ukrainian") return LANG_UKRAINIAN;
	if (lang == "belarusian") return LANG_BELARUSIAN;
	if (lang == "slovenian") return LANG_SLOVENIAN;
	if (lang == "estonian") return LANG_ESTONIAN;
	if (lang == "latvian") return LANG_LATVIAN;
	if (lang == "lithuanian") return LANG_LITHUANIAN;
	if (lang == "farsi") return LANG_FARSI;
	if (lang == "vietnamese") return LANG_VIETNAMESE;
	if (lang == "armenian") return LANG_ARMENIAN;
	if (lang == "azeri") return LANG_AZERI;
	if (lang == "basque") return LANG_BASQUE;
	if (lang == "macedonian") return LANG_MACEDONIAN;
	if (lang == "afrikaans") return LANG_AFRIKAANS;
	if (lang == "georgian") return LANG_GEORGIAN;
	if (lang == "faeroese") return LANG_FAEROESE;
	if (lang == "hindi") return LANG_HINDI;
	if (lang == "malay") return LANG_MALAY;
	if (lang == "kazak") return LANG_KAZAK;
	if (lang == "kyrgyz") return LANG_KYRGYZ;
	if (lang == "swahili") return LANG_SWAHILI;
	if (lang == "uzbek") return LANG_UZBEK;
	if (lang == "tatar") return LANG_TATAR;
	if (lang == "punjabi") return LANG_PUNJABI;
	if (lang == "gujarati") return LANG_GUJARATI;
	if (lang == "tamil") return LANG_TAMIL;
	if (lang == "telugu") return LANG_TELUGU;
	if (lang == "kannada") return LANG_KANNADA;
	if (lang == "marathi") return LANG_MARATHI;
	if (lang == "sanskrit") return LANG_SANSKRIT;
	if (lang == "mongolian") return LANG_MONGOLIAN;
	if (lang == "galician") return LANG_GALICIAN;
	if (lang == "konkani") return LANG_KONKANI;
	if (lang == "syriac") return LANG_SYRIAC;
	if (lang == "divehi") return LANG_DIVEHI;
	return LANG_NEUTRAL;
}

void real_time_thread::set_language(std::string lang) {
	WORD wLang = get_language(lang);
	if (wLang == LANG_NEUTRAL)
		info.dwLang = MAKELANGID(wLang, SUBLANG_DEFAULT);
	else
		info.dwLang = MAKELANGID(wLang, SUBLANG_NEUTRAL);
}

void real_time_thread::process_no_events(std::wstring alias) {
	std::wstring response;
	if (alias.empty())
		alias = info.alias;
	if (!nscapi::core_helper::submit_simple_message(info.target, alias, NSCAPI::returnOK, info.ok_msg, info.perf_msg, response)) {
		NSC_LOG_ERROR(_T("Failed to submit evenhtlog result: ") + response);
	}
}

void real_time_thread::process_record(std::wstring alias, const EventLogRecord &record) {
	std::wstring response;
	std::wstring message = record.render(true, info.syntax, DATE_FORMAT, info.dwLang);
	if (alias.empty())
		alias = info.alias;
	if (!nscapi::core_helper::submit_simple_message(info.target, alias, NSCAPI::returnCRIT, message, info.perf_msg, response)) {
		NSC_LOG_ERROR(_T("Failed to submit evenhtlog result: ") + response);
	}

	if (cache_) {
		boost::unique_lock<boost::timed_mutex> lock(cache_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!lock.owns_lock()) {
			NSC_LOG_ERROR(_T("ERROR: Could not get CheckEventLogCache mutex."));
			return;
		}
		hit_cache_.push_back(message);
	}
}
bool real_time_thread::check_cache(unsigned long &count, std::wstring &messages) {
	if (!cache_) {
		messages = _T("ERROR: Cache is not enabled!");
		NSC_LOG_ERROR(messages);
		return false;
	}
	boost::unique_lock<boost::timed_mutex> lock(cache_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock()) {
		messages = _T("ERROR: Could not get CheckEventLogCache mutex.");
		NSC_LOG_ERROR(messages);
		return false;
	}
	BOOST_FOREACH(const std::wstring &s, hit_cache_) {
		if (!messages.empty())
			messages += _T(", ");
		messages += s;
	}
	count = hit_cache_.size();
	hit_cache_.clear();
	return true;
}
void real_time_thread::debug_miss(const EventLogRecord &record) {
	std::wstring message = record.render(true, info.syntax, DATE_FORMAT, info.dwLang);
	NSC_DEBUG_MSG_STD(_T("No filter matched: ") + message);
}

void real_time_thread::thread_proc() {

	std::list<eventlog_filter::filter_engine> filters;
	BOOST_FOREACH(const filter_container &filter, filters_) {
		eventlog_filter::filter_argument fargs = eventlog_filter::factories::create_argument(info.syntax, DATE_FORMAT);
		fargs->filter = filter.filter;
		fargs->debug = debug_;
		fargs->alias = filter.alias;
		fargs->bShowDescriptions = true;
		eventlog_filter::filter_engine engine = eventlog_filter::factories::create_engine(fargs);

		if (!engine) {
			NSC_LOG_ERROR_STD(_T("Invalid filter: ") + filter.filter);
			continue;
		}

		if (!engine->boot()) {
			NSC_LOG_ERROR_STD(_T("Error booting filter: ") + filter.filter);
			continue;
		}

		std::wstring message;
		if (!engine->validate(message)) {
			NSC_LOG_ERROR_STD(_T("Error validating filter: ") + message);
			continue;
		}
		filters.push_back(engine);
	}



	typedef boost::shared_ptr<eventlog_wrapper> eventlog_type;
	typedef std::vector<eventlog_type> eventlog_list;
	eventlog_list list;

	BOOST_FOREACH(std::wstring l, lists_) {
		eventlog_type el = eventlog_type(new eventlog_wrapper(l));
		if (!el->seek_end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find the end of eventlog: ") + l);
		} else {
			list.push_back(el);
		}
	}
	
	// TODO: add support for scanning "missed messages" at startup

	HANDLE *handles = new HANDLE[1+list.size()];
	handles[0] = stop_event_;
	for (int i=0;i<list.size();i++) {
		list[i]->notify(handles[i+1]);
	}

	DWORD dwWaitTime = max_age_;
	if (dwWaitTime > 0 && dwWaitTime < 5000)
		dwWaitTime = 5000;
	unsigned int errors = 0;
	while (true) {
		DWORD dwWaitReason = WaitForMultipleObjects(list.size()+1, handles, FALSE, dwWaitTime==0?INFINITE:dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			BOOST_FOREACH(eventlog_filter::filter_engine engine, filters) {
				process_no_events(engine->data->alias);
			}
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete [] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + list.size())) {

			eventlog_type el = list[dwWaitReason-WAIT_OBJECT_0-1];
			DWORD status = el->read_record(0, EVENTLOG_SEQUENTIAL_READ|EVENTLOG_FORWARDS_READ);
			if (ERROR_SUCCESS != status && ERROR_HANDLE_EOF != status) {
				delete [] handles;
				return;
			}

			__time64_t ltime;
			_time64(&ltime);

			EVENTLOGRECORD *pevlr = el->read_record_with_buffer();
			while (pevlr != NULL) {
				EventLogRecord elr(el->get_name(), pevlr, ltime);
				boost::shared_ptr<eventlog_filter::filter_obj> arg = boost::shared_ptr<eventlog_filter::filter_obj>(new eventlog_filter::filter_obj(elr));
				bool matched = false;

				BOOST_FOREACH(eventlog_filter::filter_engine engine, filters) {
					if (engine->match(arg)) {
						process_record(engine->data->alias, elr);
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
	}
	delete [] handles;
	return;
}


bool real_time_thread::start() {
	if (!enabled_)
		return true;
	if (!has_filters()) {
		add_realtime_filter(_T("default"), _T("type NOT IN ('success', 'info', 'auditSuccess')"));
	}

	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("EVentLogShutdown"));

	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&real_time_thread::thread_proc, this)));
	return true;
}
bool real_time_thread::stop() {
	SetEvent(stop_event_);
	if (thread_)
		thread_->join();
	return true;
}

void real_time_thread::add_realtime_filter(std::wstring key, std::wstring query) {
	filter_container c;
	if (!key.empty() && query.empty()) {
		c.filter = key;
		filters_.push_back(c);
	} else {
		c.alias = key;
		c.filter = query;
		filters_.push_back(c);
	}
}


bool CheckEventLog::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		register_command(_T("CheckEventLog"), _T("Check for errors in the event logger!"));
		register_command(_T("check_eventlog"), _T("Check for errors in the event logger!"));
		register_command(_T("checkeventlogcache"), _T("Check for errors in the event logger!"));
		register_command(_T("check_eventlog_cache"), _T("Check for errors in the event logger!"));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, _T("eventlog"));
		

		settings.alias().add_path_to_settings()
			(_T("EVENT LOG SECTION"), _T("Section for the EventLog Checker (CheckEventLog.dll)."))

			(_T("real-time"), _T("CONFIGURE REALTIME CHECKING"), _T("A set of options to configure the real time checks"))

			(_T("real-time/filters"), sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, &thread_, _1, _2)),  
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

			(_T("destination"), sh::string_fun_key<std::wstring>(boost::bind(&real_time_thread::set_destination, &thread_, _1), _T("NSCA")),
			_T("DESTINATION"), _T("The destination for intercepted messages"))

			(_T("startup age"), sh::string_fun_key<std::wstring>(boost::bind(&real_time_thread::set_start_age, &thread_, _1), _T("30m")),
			_T("STARTUP AGE"), _T("The initial age to scan when starting NSClient++"))

			(_T("maximum age"), sh::string_fun_key<std::wstring>(boost::bind(&real_time_thread::set_max_age, &thread_, _1), _T("5m")),
			_T("MAGIMUM AGE"), _T("How long before reporting \"ok\" (if this is set to off no ok will be reported only errors)"))

			(_T("filter"), sh::string_fun_key<std::wstring>(boost::bind(&real_time_thread::set_filter, &thread_, _1), _T("")),
			_T("STARTUP AGE"), _T("The initial age to scan when starting NSClient++"))

			(_T("syntax"), sh::wstring_key(&thread_.info.syntax, _T("%type% %source%: %message%")),
			_T("STARTUP AGE"), _T("The initial age to scan when starting NSClient++"))

			(_T("language"), sh::string_fun_key<std::string>(boost::bind(&real_time_thread::set_language, &thread_, _1), ""),
			_T("MESSAGE LANGUAGE"), _T("The language to use for rendering message (mainly used fror testing)"))

			(_T("log"), sh::string_fun_key<std::wstring>(boost::bind(&real_time_thread::set_eventlog, &thread_, _1), _T("application")),
			_T("LOGS TO CHECK"), _T("Coma separated list of logs to check"))

			(_T("debug"), sh::bool_key(&thread_.debug_, false),
			_T("DEBUG"), _T("Log missed records (usefull to detect issues with filters) not usefull in production as it is a bit of a resource hog."))

			(_T("enable active"), sh::bool_key(&thread_.cache_, false),
			_T("ENABLE ACTIVE MONITORING"), _T("This will store all matches so you can use real-time filters from active monitoring (use CheckEventlogCache)."))

			(_T("ok message"), sh::wstring_key(&thread_.info.ok_msg, _T("eventlog found no records")),
			_T("OK MESSAGE"), _T("This is the message sent periodically whenever no error is discovered."))

			(_T("alias"), sh::wstring_key(&thread_.info.alias, _T("eventlog")),
			_T("ALIAS"), _T("The alias to use for this event (in NSCA this constitutes the service name)."))
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

NSCAPI::nagiosReturn CheckEventLog::checkCache(std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {

	EventLogQuery1Container query1;
	EventLogQuery2Container query2;
	bool bPerfData = true;
	unsigned int truncate = 0;
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;

	try {
		MAP_OPTIONS_BEGIN(arguments)
			MAP_OPTIONS_NUMERIC_ALL(query1, _T(""))
			MAP_OPTIONS_EXACT_NUMERIC_ALL(query2, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
		MAP_OPTIONS_END()
	} catch (checkHolders::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = _T("Invalid command line!");
		return NSCAPI::returnUNKNOWN;
	}

	unsigned long count = 0;
	if (!thread_.check_cache(count, message)) {
		return NSCAPI::returnUNKNOWN;
	}

	if (!bPerfData) {
		query1.perfData = false;
		query2.perfData = false;
	}
	if (query1.alias.empty())
		query1.alias = _T("eventlog");
	if (query2.alias.empty())
		query2.alias = _T("eventlog");
	if (query1.hasBounds())
		query1.runCheck(count, returnCode, message, perf);
	else if (query2.hasBounds())
		query2.runCheck(count, returnCode, message, perf);
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

NSCAPI::nagiosReturn CheckEventLog::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("checkeventlogcache") || command == _T("check_eventlog_cache"))
		return checkCache(arguments, message, perf);
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
	if (command == _T("insert-eventlog-message") || command == _T("insert-eventlog") || command == _T("insert-message") || command == _T("insert")) {
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

		if (help) {
			std::stringstream ss;
			ss << "CheckEventLog Command line syntax:" << std::endl;
			ss << desc;
			message = utf8::cvt<std::wstring>(ss.str());
			return NSCAPI::isSuccess;
		} else {
			event_source source(source_name);
			WORD dwType = EventLogRecord::translateType(type);
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

			if (!ReportEvent(source, dwType, category, tID, NULL, strings.size(), 0, string_data, NULL)) {
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
NSC_WRAPPERS_MAIN_DEF(CheckEventLog);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
