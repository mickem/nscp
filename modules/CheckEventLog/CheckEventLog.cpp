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

#include <boost/foreach.hpp>
#include <boost/program_options.hpp>


#include "CheckEventLog.h"
#include <filter_framework.hpp>

#include <time.h>
#include <utils.h>
#include <error.hpp>
#include <map>
#include <vector>

#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>

#include "filter.hpp"
#include "filters.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <simple_timer.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

#include "simple_registry.hpp"
#include "eventlog_record.hpp"
#include "real_time_thread.hpp"

struct parse_exception {
	parse_exception(std::wstring) {}
};


bool CheckEventLog::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, _T("eventlog"));
		
	thread_.reset(new real_time_thread());
	if (!thread_) {
		NSC_LOG_ERROR_STD(_T("Failed to create thread container"));
		return false;
	}
	thread_->filters_path_ = settings.alias().get_settings_path(_T("real-time/filters"));


	settings.alias().add_path_to_settings()
		(_T("EVENT LOG SECTION"), _T("Section for the EventLog Checker (CheckEventLog.dll)."))

		(_T("real-time"), _T("CONFIGURE REALTIME CHECKING"), _T("A set of options to configure the real time checks"))

		(_T("real-time/filters"), sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, thread_, get_settings_proxy(), _1, _2)),  
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

		(_T("enabled"), sh::bool_fun_key<bool>(boost::bind(&real_time_thread::set_enabled, thread_, _1), false),
		_T("REAL TIME CHECKING"), _T("Spawns a backgrounnd thread which detects issues and reports them back instantly."))

		(_T("startup age"), sh::string_fun_key<std::wstring>(boost::bind(&real_time_thread::set_start_age, thread_, _1), _T("30m")),
		_T("STARTUP AGE"), _T("The initial age to scan when starting NSClient++"))

		(_T("log"), sh::wstring_key(&thread_->logs_ ,_T("application,system")),
		_T("LOGS TO CHECK"), _T("Comma separated list of logs to check"))

		(_T("debug"), sh::bool_key(&thread_->debug_, false),
		_T("DEBUG"), _T("Log missed records (usefull to detect issues with filters) not usefull in production as it is a bit of a resource hog."))

		;

	settings.register_all();
	settings.notify();

	if (mode == NSCAPI::normalStart) {
		if (!thread_->start())
			NSC_LOG_ERROR_STD(_T("Failed to start collection thread"));
	}
	return true;
}
bool CheckEventLog::unloadModule() {
	if (!thread_->stop())
		NSC_LOG_ERROR_STD(_T("Failed to start collection thread"));
	return true;
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

inline std::time_t to_time_t(boost::posix_time::ptime t) { 
	if( t == boost::date_time::neg_infin ) 
		return 0; 
	else if( t == boost::date_time::pos_infin ) 
		return LONG_MAX; 
	boost::posix_time::ptime start(boost::gregorian::date(1970,1,1)); 
	return (t-start).total_seconds(); 
} 

inline long long parse_time(std::wstring time) {
	long long now = to_time_t(boost::posix_time::second_clock::universal_time());
	std::wstring::size_type p = time.find_first_not_of(_T("-0123456789"));
	if (p == std::wstring::npos)
		return now + boost::lexical_cast<long long>(time);
	long long value = boost::lexical_cast<long long>(time.substr(0, p));
	if ( (time[p] == 's') || (time[p] == 'S') )
		return now + value;
	else if ( (time[p] == 'm') || (time[p] == 'M') )
		return now + (value * 60);
	else if ( (time[p] == 'h') || (time[p] == 'H') )
		return now + (value * 60 * 60);
	else if ( (time[p] == 'd') || (time[p] == 'D') )
		return now + (value * 24 * 60 * 60);
	else if ( (time[p] == 'w') || (time[p] == 'W') )
		return now + (value * 7 * 24 * 60 * 60);
	return now + value;
}


typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsULongInteger> EventLogQuery1Container;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULongInteger> EventLogQuery2Container;

void CheckEventLog::check_eventlog(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	simple_timer time;
	
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;

	std::vector<std::wstring> files;
	EventLogQuery1Container query1;
	EventLogQuery2Container query2;


	eventlog_filter::filter_argument fargs = eventlog_filter::factories::create_argument(syntax_, DATE_FORMAT);

	bool bPerfData = true;
	bool unique = false;
	unsigned int truncate = 0;
	event_log_buffer buffer(buffer_length_);
	std::wstring scan_range;

	po::options_description desc = nscapi::program_options::create_desc(request);
	desc.add_options()
		("truncate", po::value<unsigned int>(&truncate),			"Truncate the resulting message (mainly useful in older version of nsclient++)")
		("filter", po::wvalue<std::wstring>(&fargs->filter),		"Filter which marks interesting items.\nInteresting items are items which will be included in the check. They do not denote warning or critical state but they are checked use this to filter out unwanted items.")
		("file", po::wvalue<std::vector<std::wstring>>(&files),		"The name of an eventlog file the default ones are Application, Security and System. If the specified eventlog was not found due to some idiotic reason windows opens the \"application\" log instead.")
		("syntax", po::wvalue<std::wstring>(&fargs->syntax)->default_value(syntax_), "A string to use to represent each matched eventlog entry the following keywords will be replaced with corresponding values: %source%, %generated%, %written%, %type%, %severity%, %strings%, %id% and %message% (%message% requires you to set the description flag) %count% (requires the unique flag) can be used to display a count of the records returned.")
		("date-syntax", po::wvalue<std::wstring>(&fargs->date_syntax)->default_value(DATE_FORMAT), "Syntax of renderd dates.")
		("scan-range", po::wvalue<std::wstring>(&scan_range), "Date range to scan.\nThis is the approximate dates to search through this speeds up searching a lot but there is no guarantee messages are ordered.")
		("debug", po::bool_switch(&fargs->debug), "Enable debug information.")
		("descriptions", po::bool_switch(&fargs->debug), "Allow searching and scanning and rendering descriptions field (will be much slower).")
		("unique", po::bool_switch(&unique), "Only return one of each message (based on message id and source).")
		;

	nscapi::program_options::legacy::add_numerical_all(desc);
	nscapi::program_options::legacy::add_exact_numerical_all(desc);
	nscapi::program_options::legacy::add_ignore_perf_data(desc, bPerfData);
	nscapi::program_options::legacy::add_show_all(desc);

	boost::program_options::variables_map vm;
	nscapi::program_options::unrecognized_map unrecognized;
	if (!nscapi::program_options::process_arguments_unrecognized(vm, unrecognized, desc, request, *response)) 
		return;
	nscapi::program_options::legacy::collect_numerical_all(vm, query1);
	nscapi::program_options::legacy::collect_exact_numerical_all(vm, query2);
	nscapi::program_options::legacy::collect_show_all(vm, query1);
	nscapi::program_options::legacy::collect_show_all(vm, query2);
	nscapi::program_options::alias_map aliases = nscapi::program_options::parse_legacy_alias(unrecognized, "alias");

	unsigned long int hit_count = 0;
	if (files.empty())
		return nscapi::protobuf::functions::set_response_bad(*response, "No file specified try adding: file=Application");
	if (!query1.hasBounds() && !query2.hasBounds())
		return nscapi::protobuf::functions::set_response_bad(*response, "No bounds specified try adding MaxWarn=1");

	bool buffer_error_reported = false;


	eventlog_filter::filter_engine impl = eventlog_filter::factories::create_engine(fargs);

	if (!impl)
		return nscapi::protobuf::functions::set_response_bad(*response, "Failed to initialize filter subsystem.");

	impl->boot();

	__time64_t ltime;
	_time64(&ltime);

	std::wstring tmp;
	if (!impl->validate(tmp)) {
		return nscapi::protobuf::functions::set_response_bad(*response, utf8::cvt<std::string>(tmp));
	}

	NSC_DEBUG_MSG_STD(_T("Boot time: ") + strEx::itos(time.stop()));

	std::wstring message, perf;
	BOOST_FOREACH(const std::wstring &file, files) {
		std::wstring name = file;
		if (lookup_names_) {
			name = eventlog_wrapper::find_eventlog_name(name);
			if (file != name) {
				NSC_DEBUG_MSG_STD(_T("Opening alternative log: ") + name);
			}
		}
		HANDLE hLog = OpenEventLog(NULL, name.c_str());
		if (hLog == NULL)
			return nscapi::protobuf::functions::set_response_bad(*response, "Could not open the '" + utf8::cvt<std::string>(name) + "' event log: "  + utf8::cvt<std::string>(error::lookup::last_error()));
		uniq_eventlog_map uniq_records;
		unsigned long long stop_date;
		enum direction_type {
			direction_none, direction_forwards, direction_backwards

		};
		direction_type direction = direction_none;
		DWORD flags = EVENTLOG_SEQUENTIAL_READ;
		if ((scan_range.size() > 0) && (scan_range[0] == L'-')) {
			direction = direction_backwards;
			flags|=EVENTLOG_BACKWARDS_READ;
			stop_date = parse_time(scan_range);
		} else if (scan_range.size() > 0) {
			direction = direction_forwards;
			flags|=EVENTLOG_FORWARDS_READ;
			stop_date = parse_time(scan_range);
		} else {
			flags|=EVENTLOG_FORWARDS_READ;
		}

		DWORD dwRead, dwNeeded;
		bool is_scanning = true;
		while (is_scanning) {

			BOOL bStatus = ReadEventLog(hLog, flags,
				0, buffer.getBufferUnsafe(), buffer.getBufferSize(), &dwRead, &dwNeeded);
			if (bStatus == FALSE) {
				DWORD err = GetLastError();
				if (err == ERROR_INSUFFICIENT_BUFFER) {
					if (!buffer_error_reported) {
						NSC_LOG_ERROR_STD(_T("EvenlogBuffer is too small change the value of buffer_length=") + strEx::itos(dwNeeded+1) + _T(": ") + error::lookup::last_error(err));
						buffer_error_reported = true;
					}
				} else if (err == ERROR_HANDLE_EOF) {
					is_scanning = false;
					break;
				} else {
					std::wstring error_msg = error::lookup::last_error(err);
					CloseEventLog(hLog);
					return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read from event log: " + utf8::cvt<std::string>(error_msg));
				}
			}
			EVENTLOGRECORD *pevlr = buffer.getBufferUnsafe(); 
			while (dwRead > 0) { 
				EventLogRecord record(file, pevlr, ltime);
				if (direction == direction_backwards && record.written() < stop_date) {
					is_scanning = false;
					break;
				}
				if (direction == direction_forwards && record.written() > stop_date) {
					is_scanning = false;
					break;
				}
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
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("Eventlog check ok");
	response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(returnCode));
	response->set_message(utf8::cvt<std::string>(message));
}

NSCAPI::nagiosReturn CheckEventLog::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	if (command == _T("insert-eventlog-message") || command == _T("insert-eventlog") || command == _T("insert-message") || command == _T("insert") || command.empty()) {
		//std::vector<std::wstring> args(data.args.begin(), data.args.end());
		bool ok = insert_eventlog(arguments, result);
		return ok?NSCAPI::isSuccess:NSCAPI::hasFailed;
	} else if (command == _T("help")) {
		std::list<std::wstring> args;
		args.push_back(_T("--help"));
		insert_eventlog(args, result);
		return NSCAPI::isSuccess;
	}
	return NSCAPI::returnIgnored;
}



NSCAPI::nagiosReturn CheckEventLog::insert_eventlog(std::list<std::wstring> arguments, std::wstring &message) {
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

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).run();
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
