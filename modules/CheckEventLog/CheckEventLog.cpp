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

//#include <combaseapi.h>
#include <Sddl.h>

#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

#include "CheckEventLog.h"

#include <time.h>
#include <error.hpp>
#include <map>
#include <vector>
#include <winevt.h>

#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>

#include "filter.hpp"

#include <parsers/filter/cli_helper.hpp>
#include <buffer.hpp>
#include <handle.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

#include "simple_registry.hpp"
#include "eventlog_record.hpp"
#include "realtime_thread.hpp"

struct parse_exception {
	parse_exception(std::wstring) {}
};



bool CheckEventLog::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	eventlog::api::load_procs();

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, "eventlog");
		
	thread_.reset(new real_time_thread());
	if (!thread_) {
		NSC_LOG_ERROR_STD("Failed to create thread container");
		return false;
	}
	thread_->filters_path_ = settings.alias().get_settings_path("real-time/filters");


	settings.alias().add_path_to_settings()
		("EVENT LOG SECTION", "Section for the EventLog Checker (CheckEventLog.dll).")

		("real-time", "CONFIGURE REALTIME CHECKING", "A set of options to configure the real time checks")

		("real-time/filters", sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, thread_, get_settings_proxy(), _1, _2)),  
		"REALTIME FILTERS", "A set of filters to use in real-time mode")
		;

	settings.alias().add_key_to_settings()
		("debug", sh::bool_key(&debug_, false),
		"DEBUG", "Log more information when filtering (useful to detect issues with filters) not useful in production as it is a bit of a resource hog.")

		("lookup names", sh::bool_key(&lookup_names_, true),
		"LOOKUP NAMES", "Lookup the names of eventlog files")

		("syntax", sh::string_key(&syntax_),
		"SYNTAX", "Set this to use a specific syntax string for all commands (that don't specify one).")

		("buffer size", sh::int_key(&buffer_length_, 128*1024),
		"BUFFER_SIZE", "The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can recieve.")

		;

	settings.alias().add_key_to_settings("real-time")

		("enabled", sh::bool_fun_key<bool>(boost::bind(&real_time_thread::set_enabled, thread_, _1), false),
		"REAL TIME CHECKING", "Spawns a background thread which detects issues and reports them back instantly.")

		("startup age", sh::string_fun_key<std::string>(boost::bind(&real_time_thread::set_start_age, thread_, _1), "30m"),
		"STARTUP AGE", "The initial age to scan when starting NSClient++")

		("log", sh::string_key(&thread_->logs_ , "application,system"),
		"LOGS TO CHECK", "Comma separated list of logs to check")

		("debug", sh::bool_key(&thread_->debug_, false),
		"DEBUG", "Log missed records (useful to detect issues with filters) not useful in production as it is a bit of a resource hog.")

		;

	settings.register_all();
	settings.notify();

	if (mode == NSCAPI::normalStart) {
		if (!thread_->start())
			NSC_LOG_ERROR_STD("Failed to start collection thread");
	}
	return true;
}
bool CheckEventLog::unloadModule() {
	if (!thread_->stop())
		NSC_LOG_ERROR_STD("Failed to start collection thread");
	return true;
}

class uniq_eventlog_record {
	DWORD ID;
	WORD type;
	WORD category;
public:
	std::string message;
	uniq_eventlog_record(EVENTLOGRECORD *pevlr) : ID(pevlr->EventID&0xffff), type(pevlr->EventType), category(pevlr->EventCategory) {}
	bool operator< (const uniq_eventlog_record &other) const { 
		return (ID < other.ID) || ((ID==other.ID)&&(type < other.type)) || (ID==other.ID&&type==other.type)&&(category < other.category);
	}
	std::wstring to_string() const {
		return _T("id=") + strEx::itos(ID) + _T("type=") + strEx::itos(type) + _T("category=") + strEx::itos(category);
	}
};
typedef std::map<uniq_eventlog_record,unsigned int> uniq_eventlog_map;
typedef hlp::buffer<BYTE, EVENTLOGRECORD*> eventlog_buffer;

inline std::time_t to_time_t(boost::posix_time::ptime t) { 
	if( t == boost::date_time::neg_infin ) 
		return 0; 
	else if( t == boost::date_time::pos_infin ) 
		return LONG_MAX; 
	boost::posix_time::ptime start(boost::gregorian::date(1970,1,1)); 
	return (t-start).total_seconds(); 
} 

inline long long parse_time(std::string time) {
	long long now = to_time_t(boost::posix_time::second_clock::universal_time());
	std::string::size_type p = time.find_first_not_of("-0123456789");
	if (p == std::string::npos)
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

void check_legacy(const std::string &logfile, std::string &scan_range, eventlog_filter::filter &filter)  {
	typedef eventlog_filter::filter filter_type;
	eventlog_buffer buffer(4096);
	HANDLE hLog = OpenEventLog(NULL, utf8::cvt<std::wstring>(logfile).c_str());
	if (hLog == NULL)
		throw nscp_exception("Could not open the '" + logfile + "' event log: "  + error::lookup::last_error());
	uniq_eventlog_map uniq_records;
	long long stop_date;
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

		BOOL bStatus = ReadEventLog(hLog, flags, 0, buffer.get(), buffer.size(), &dwRead, &dwNeeded);
		if (bStatus == FALSE) {
			DWORD err = GetLastError();
			if (err == ERROR_INSUFFICIENT_BUFFER) {
				buffer.resize(dwNeeded);
				if (!ReadEventLog(hLog, flags, 0, buffer.get(), buffer.size(), &dwRead, &dwNeeded))
					throw nscp_exception("Error reading eventlog: " + error::lookup::last_error());
			} else if (err == ERROR_HANDLE_EOF) {
				is_scanning = false;
				break;
			} else {
				std::string error_msg = error::lookup::last_error(err);
				CloseEventLog(hLog);
				throw nscp_exception("Failed to read from event log: " + error_msg);
			}
		}
		__time64_t ltime;
		_time64(&ltime);

		EVENTLOGRECORD *pevlr = buffer.get(); 
		while (dwRead > 0) { 
			EventLogRecord record(logfile, pevlr, ltime);
			if (direction == direction_backwards && record.written() < stop_date) {
				is_scanning = false;
				break;
			}
			if (direction == direction_forwards && record.written() > stop_date) {
				is_scanning = false;
				break;
			}
			boost::tuple<bool,bool> ret = filter.match(filter_type::object_type(new eventlog_filter::old_filter_obj(record)));
			if (ret.get<1>()) {
				break;
			}
			// 				bool match = impl->match(arg);
			// 				if (match&&unique) {
			// 					match = false;
			// 					uniq_eventlog_record uniq_record = pevlr;
			// 					uniq_eventlog_map::iterator it = uniq_records.find(uniq_record);
			// 					if (it != uniq_records.end()) {
			// 						(*it).second ++;
			// 					}
			// 					else {
			// 						uniq_record.message = record.render(fargs->bShowDescriptions, fargs->syntax);
			// 						uniq_records[uniq_record] = 1;
			// 					}
			// 					hit_count++;
			// 				} else if (match) {
			// 					if (!fargs->syntax.empty()) {
			// 						strEx::append_list(message, record.render(fargs->bShowDescriptions, fargs->syntax));
			// 					}
			// 					hit_count++;
			// 				}
			dwRead -= pevlr->Length; 
			pevlr = reinterpret_cast<EVENTLOGRECORD*>((LPBYTE)pevlr + pevlr->Length); 
		} 
	}
	CloseEventLog(hLog);
}


void check_modern(const std::string &logfile, std::string &scan_range, eventlog_filter::filter &filter)  {
	typedef eventlog_filter::filter filter_type;
	DWORD status = ERROR_SUCCESS;
	const int batch_size = 10;	// TODO make configurable
	LPWSTR pwsQuery = L"*";	// TODO make configurable
	long long stop_date;
	enum direction_type {
		direction_none, direction_forwards, direction_backwards
	};
	direction_type direction = direction_none;
	DWORD flags = eventlog::api::EvtQueryChannelPath;
	if ((scan_range.size() > 0) && (scan_range[0] == L'-')) {
		direction = direction_backwards;
		flags |= eventlog::api::EvtQueryReverseDirection;
		stop_date = parse_time(scan_range);
	} else if (scan_range.size() > 0) {
		direction = direction_forwards;
		flags |= eventlog::api::EvtQueryForwardDirection;
		stop_date = parse_time(scan_range);
	}
	eventlog::evt_handle hResults = eventlog::EvtQuery(NULL, utf8::cvt<std::wstring>(logfile).c_str(), pwsQuery, flags);
	if (!hResults)
		throw nscp_exception("Failed to open channel: " + error::lookup::last_error(status));
	status = GetLastError();
	if (status == ERROR_EVT_CHANNEL_NOT_FOUND)
		throw nscp_exception("Channel not found: " + error::lookup::last_error(status));
	else if (status == ERROR_EVT_INVALID_QUERY)
		throw nscp_exception("Invalid query: " + error::lookup::last_error(status));
	else if (status != ERROR_SUCCESS)
		throw nscp_exception("EvtQuery failed: " + error::lookup::last_error(status));


	eventlog::evt_handle hContext = eventlog::EvtCreateRenderContext(0, NULL, eventlog::api::EvtRenderContextSystem);
	if (!hContext)
		throw nscp_exception("EvtCreateRenderContext failed: " + error::lookup::last_error());

	while (true) {
		DWORD status = ERROR_SUCCESS;
		hlp::buffer<eventlog::api::EVT_HANDLE> hEvents(batch_size);
		DWORD dwReturned = 0;

		__time64_t ltime;
		_time64(&ltime);

		while (true) {
			if (!eventlog::EvtNext(hResults, batch_size, hEvents, 100, 0, &dwReturned)) {
				status = GetLastError();
				if (status == ERROR_NO_MORE_ITEMS || status == ERROR_TIMEOUT)
					return;
				else if (status != ERROR_SUCCESS)
					throw nscp_exception("EvtNext failed: " + error::lookup::last_error(status));
			}
			for (DWORD i = 0; i < dwReturned; i++) {
				eventlog::evt_handle handle(hEvents[i]);
				try {
					filter_type::object_type item(new eventlog_filter::new_filter_obj(logfile, handle, hContext));
					if (direction == direction_backwards && item->get_written() < stop_date)
						return;
					if (direction == direction_forwards && item->get_written() > stop_date)
						return;
					boost::tuple<bool,bool> ret = filter.match(item);
					if (ret.get<1>()) {
						break;
					}
				} catch (const nscp_exception &e) {
					NSC_LOG_ERROR("Failed to describe event: " + e.reason());
				} catch (...) {
					NSC_LOG_ERROR("Failed to describe event");
				}
			}
		}


			// 				bool match = impl->match(arg);
			// 				if (match&&unique) {
			// 					match = false;
			// 					uniq_eventlog_record uniq_record = pevlr;
			// 					uniq_eventlog_map::iterator it = uniq_records.find(uniq_record);
			// 					if (it != uniq_records.end()) {
			// 						(*it).second ++;
			// 					}
			// 					else {
			// 						uniq_record.message = record.render(fargs->bShowDescriptions, fargs->syntax);
			// 						uniq_records[uniq_record] = 1;
			// 					}
			// 					hit_count++;
			// 				} else if (match) {
			// 					if (!fargs->syntax.empty()) {
			// 						strEx::append_list(message, record.render(fargs->bShowDescriptions, fargs->syntax));
			// 					}
			// 					hit_count++;
			// 				}
	} 
}

void CheckEventLog::check_eventlog(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef eventlog_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;
	std::string scan_range;

	filter_type filter;
	filter_helper.add_options(filter.get_filter_syntax());
	filter_helper.add_syntax("${file}: ${count} (${problem_list})", filter.get_format_syntax(), "${file} ${source} (${message})", "${file}_${source}");
	filter_helper.get_desc().add_options()
		("file", po::value<std::vector<std::string> >(&file_list),	"File to read (can be specified multiple times to check multiple files.\nNotice that specifying multiple files will create an aggregate set you will not check each file individually."
		"In other words if one file contains an error the entire check will result in error.")
		("scan-range", po::value<std::string>(&scan_range), "Date range to scan.\nThis is the approximate dates to search through this speeds up searching a lot but there is no guarantee messages are ordered.")
		;
	if (!filter_helper.parse_options())
		return;

	if (filter_helper.empty()) {
		data.filter_string = "level in ('error', 'warning')";
		filter_helper.set_default("count > 0", "count > 5");
		scan_range = "-24h";
	}
	if (file_list.empty()) {
		file_list.push_back("Application");
		file_list.push_back("System");
	}

	if (!filter_helper.build_filter(filter))
		return;


// 	desc.add_options()
// 		("unique", po::bool_switch(&unique), "Only return one of each message (based on message id and source).")
// 		;

	BOOST_FOREACH(const std::string &file, file_list) {
		std::string name = file;
		if (lookup_names_) {
			name = eventlog_wrapper::find_eventlog_name(name);
			if (file != name) {
				NSC_DEBUG_MSG_STD("Opening alternative log: " + utf8::cvt<std::string>(name));
			}
		}
		if (eventlog::api::supports_modern())
			check_modern(name, scan_range, filter);
		else
			check_legacy(name, scan_range, filter);
// 		BOOST_FOREACH(const uniq_eventlog_map::value_type &v, uniq_records) {
// 			std::string msg = v.first.message;
// 			strEx::replace(msg, "%count%", strEx::s::xtos(v.second));
// 			strEx::append_list(message, msg);
// 		}
	}
	modern_filter::perf_writer writer(response);
	filter_helper.post_process(filter, &writer);
}



bool CheckEventLog::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	if (request.command() == "insert-message" || request.command() == "insert") {
		insert_eventlog(request, response);
		return true;
	} else if (request.command() == "list-providers" || request.command() == "list") {
		list_providers(request, response);
		return true;
	}
	return false;
}


void PrintEventSystemData(eventlog::evt_handle &hEvent) {
	hlp::buffer<wchar_t, PVOID> buffer(1024);
	DWORD dwBufferSize = 0;
 	DWORD dwPropertyCount = 0;
// 	LPWSTR pwsSid = NULL;
// 	ULONGLONG ullTimeStamp = 0;
// 	ULONGLONG ullNanoseconds = 0;
// 	SYSTEMTIME st;
// 	FILETIME ft;

	eventlog::evt_handle hContext = eventlog::EvtCreateRenderContext(0, NULL, eventlog::api::EvtRenderContextSystem);
	if (!hContext)
		throw nscp_exception("EvtCreateRenderContext failed: " + error::lookup::last_error());

	if (!EvtRender(hContext, hEvent, eventlog::api::EvtRenderEventValues, buffer.size(), buffer.get(), &dwBufferSize, &dwPropertyCount)) {
		DWORD status = GetLastError();
		if (status == ERROR_INSUFFICIENT_BUFFER) {
			buffer.resize(dwBufferSize);
			if (!EvtRender(hContext, hEvent, eventlog::api::EvtRenderEventValues, buffer.size(), buffer.get(), &dwBufferSize, &dwPropertyCount))
				throw nscp_exception("EvtRender failed: " + error::lookup::last_error());
		}
	}
	eventlog::api::PEVT_VARIANT pRenderedValues = buffer.get_t<eventlog::api::PEVT_VARIANT>();
	NSC_DEBUG_MSG("Provider Name: " + utf8::cvt<std::string>(pRenderedValues[eventlog::api::EvtSystemProviderName].StringVal));
// 	if (pRenderedValues[eventlog::api::EvtSystemProviderGuid].GuidVal != NULL)
// 	{
// 		WCHAR wsGuid[50];
// 		StringFromGUID2(*(pRenderedValues[eventlog::api::EvtSystemProviderGuid].GuidVal), wsGuid, sizeof(wsGuid)/sizeof(WCHAR));
// 		NSC_DEBUG_MSG("Provider Guid: " + utf8::cvt<std::string>(wsGuid));
// 	}

	DWORD EventID = pRenderedValues[eventlog::api::EvtSystemEventID].UInt16Val;
	if (pRenderedValues[eventlog::api::EvtSystemQualifiers].Type != eventlog::api::EvtVarTypeNull)
		NSC_DEBUG_MSG("EventID: " + strEx::s::xtos(MAKELONG(pRenderedValues[eventlog::api::EvtSystemEventID].UInt16Val, pRenderedValues[eventlog::api::EvtSystemQualifiers].UInt16Val)));

// 	wprintf(L"Version: %u\n", (api::EvtVarTypeNull == pRenderedValues[api::EvtSystemVersion].Type) ? 0 : pRenderedValues[api::EvtSystemVersion].ByteVal);
// 	wprintf(L"Level: %u\n", (api::EvtVarTypeNull == pRenderedValues[api::EvtSystemLevel].Type) ? 0 : pRenderedValues[api::EvtSystemLevel].ByteVal);
// 	wprintf(L"Task: %hu\n", (api::EvtVarTypeNull == pRenderedValues[api::EvtSystemTask].Type) ? 0 : pRenderedValues[api::EvtSystemTask].UInt16Val);
// 	wprintf(L"Opcode: %u\n", (api::EvtVarTypeNull == pRenderedValues[api::EvtSystemOpcode].Type) ? 0 : pRenderedValues[api::EvtSystemOpcode].ByteVal);
// 	wprintf(L"Keywords: 0x%I64x\n", pRenderedValues[api::EvtSystemKeywords].UInt64Val);

// 	ullTimeStamp = pRenderedValues[api::EvtSystemTimeCreated].FileTimeVal;
// 	ft.dwHighDateTime = (DWORD)((ullTimeStamp >> 32) & 0xFFFFFFFF);
// 	ft.dwLowDateTime = (DWORD)(ullTimeStamp & 0xFFFFFFFF);
// 
// 	FileTimeToSystemTime(&ft, &st);
// 	ullNanoseconds = (ullTimeStamp % 10000000) * 100; // Display nanoseconds instead of milliseconds for higher resolution
// 	wprintf(L"TimeCreated SystemTime: %02d/%02d/%02d %02d:%02d:%02d.%I64u)\n", 
// 		st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond, ullNanoseconds);
// 
// 	wprintf(L"EventRecordID: %I64u\n", pRenderedValues[api::EvtSystemEventRecordId].UInt64Val);
// 
// 	if (api::EvtVarTypeNull != pRenderedValues[api::EvtSystemActivityID].Type)
// 	{
// 		StringFromGUID2(*(pRenderedValues[api::EvtSystemActivityID].GuidVal), wsGuid, sizeof(wsGuid)/sizeof(WCHAR));
// 		wprintf(L"Correlation ActivityID: %s\n", wsGuid);
// 	}
// 
// 	if (api::EvtVarTypeNull != pRenderedValues[api::EvtSystemRelatedActivityID].Type)
// 	{
// 		StringFromGUID2(*(pRenderedValues[api::EvtSystemRelatedActivityID].GuidVal), wsGuid, sizeof(wsGuid)/sizeof(WCHAR));
// 		wprintf(L"Correlation RelatedActivityID: %s\n", wsGuid);
// 	}
// 
// 	wprintf(L"Execution ProcessID: %lu\n", pRenderedValues[api::EvtSystemProcessID].UInt32Val);
// 	wprintf(L"Execution ThreadID: %lu\n", pRenderedValues[api::EvtSystemThreadID].UInt32Val);
// 	wprintf(L"Channel: %s\n", (api::EvtVarTypeNull == pRenderedValues[api::EvtSystemChannel].Type) ? L"" : pRenderedValues[api::EvtSystemChannel].StringVal);
// 	wprintf(L"Computer: %s\n", pRenderedValues[api::EvtSystemComputer].StringVal);
// 
// 	if (api::EvtVarTypeNull != pRenderedValues[api::EvtSystemUserID].Type)
// 	{
// 		if (ConvertSidToStringSid(pRenderedValues[api::EvtSystemUserID].SidVal, &pwsSid))
// 		{
// 			wprintf(L"Security UserID: %s\n", pwsSid);
// 			LocalFree(pwsSid);
// 		}
// 	}

// 	evt_handle hMetadata = EvtOpenPublisherMetadata(NULL, pRenderedValues[api::EvtSystemProviderName].StringVal, NULL, 0, 0);
// 	if (!hMetadata)
// 		throw nscp_exception("EvtOpenPublisherMetadata failed: " + error::lookup::last_error());
// 
// 
// 	if (!EvtFormatMessage(hMetadata, hEvent, 0, 0, NULL, api::EvtFormatMessageEvent, buffer.size(), buffer.get_t<LPWSTR>(), &dwBufferSize)) {
// 		DWORD status = GetLastError();
// 		if (status == ERROR_INSUFFICIENT_BUFFER) {
// 			buffer.resize(dwBufferSize);
// 			if (!EvtFormatMessage(hMetadata, hEvent, 0, 0, NULL, api::EvtFormatMessageEvent, buffer.size(), buffer.get_t<LPWSTR>(), &dwBufferSize))
// 				throw nscp_exception("EvtFormatMessage failed: " + error::lookup::last_error());
// 		}
// 		else if (status != ERROR_EVT_MESSAGE_NOT_FOUND  && ERROR_EVT_MESSAGE_ID_NOT_FOUND != status)
// 			throw nscp_exception("EvtFormatMessage failed: " + error::lookup::last_error(status));
// 	}
// 	NSC_DEBUG_MSG("==> " + utf8::cvt<std::string>(buffer.get_t<wchar_t*>()));
}

void PrintResults(eventlog::api::EVT_HANDLE hResults, DWORD batch_size) {
	DWORD status = ERROR_SUCCESS;
	hlp::buffer<eventlog::api::EVT_HANDLE> hEvents(batch_size);
	DWORD dwReturned = 0;

	while (true) {
		if (!eventlog::EvtNext(hResults, batch_size, hEvents, 100, 0, &dwReturned)) {
			status = GetLastError();
			if (status == ERROR_NO_MORE_ITEMS || status == ERROR_TIMEOUT)
				return;
			else if (status != ERROR_SUCCESS)
				throw nscp_exception("EvtNext failed: " + error::lookup::last_error(status));
		}
		for (DWORD i = 0; i < dwReturned; i++) {
			eventlog::evt_handle handle(hEvents[i]);
			try {
				PrintEventSystemData(handle);
			} catch (const nscp_exception &e) {
				NSC_LOG_ERROR("Failed to describe event: " + e.reason());
			} catch (...) {
				NSC_LOG_ERROR("Failed to describe event");
			}
		}
	}
}


void CheckEventLog::list_providers(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	try {
		namespace po = boost::program_options;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", po::bool_switch(), "Show help screen")
			;

		boost::program_options::variables_map vm;


		nscapi::program_options::process_arguments_from_request(vm, desc, request, *response);
		{
			eventlog::evt_handle hProviders;
			hlp::buffer<WCHAR, LPWSTR> buffer(1024);
			DWORD dwBufferSize = 0;
			DWORD status = ERROR_SUCCESS;

			hProviders = eventlog::EvtOpenChannelEnum(NULL, 0);
			if (!hProviders) {
				NSC_LOG_ERROR("EvtOpenPublisherEnum failed: ", error::lookup::last_error());
				return;
			}
			while (true) {
				if  (!eventlog::EvtNextChannelPath(hProviders, buffer.size(), buffer.get(), &dwBufferSize)) {
					status = GetLastError();
					if (ERROR_NO_MORE_ITEMS == status)
						break;
					else if (ERROR_INSUFFICIENT_BUFFER == status) {
						buffer.resize(dwBufferSize);
						if  (!eventlog::EvtNextChannelPath(hProviders, buffer.size(), buffer.get(), &dwBufferSize))
							throw nscp_exception("EvtNextChannelPath failed: " + error::lookup::last_error());
					} else if (status != ERROR_SUCCESS)
						throw nscp_exception("EvtNextChannelPath failed: " + error::lookup::last_error(status));
				}
				wprintf(L"%s\n", buffer.get());
			}
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to parse command line: ", e);
		return nscapi::protobuf::functions::set_response_bad(*response, "Error");
	}
}

void CheckEventLog::insert_eventlog(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	try {
		namespace po = boost::program_options;

		std::string type, severity;
		std::wstring source_name;
		std::vector<std::wstring> strings;
		WORD wEventID = 0, category = 0, customer = 0;
		WORD facility = 0;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", po::bool_switch(), "Show help screen")
			("source,s", po::wvalue<std::wstring>(&source_name)->default_value(_T("Application Error")), "source to use")
			("type,t", po::value<std::string>(&type), "Event type")
			("level,l", po::value<std::string>(&type), "Event level (type)")
			("facility,f", po::value<WORD>(&facility), "Facility/Qualifier")
			("qualifier,q", po::value<WORD>(&facility), "Facility/Qualifier")
			("severity", po::value<std::string>(&severity), "Event severity")
			("category,c", po::value<WORD>(&category), "Event category")
			("customer", po::value<WORD>(&customer), "Customer bit 0,1")
			("arguments,a", po::wvalue<std::vector<std::wstring> >(&strings), "Message arguments (strings)")
			("id,i", po::value<WORD>(&wEventID), "Event ID")
			;
		boost::program_options::variables_map vm;
		if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) 
			return;

		event_source source(source_name);
		WORD wType = EventLogRecord::translateType(type);
		WORD wSeverity = EventLogRecord::translateSeverity(severity);
		DWORD tID = (wEventID&0xffff) | ((facility&0xfff)<<16) | ((customer&0x1)<<29) | ((wSeverity&0x3)<<30);
		hlp::buffer<LPCWSTR> string_data(strings.size());
		int i=0;
		// TODO: FIxme this is broken!
// 		BOOST_FOREACH(const std::wstring &s, strings) {
// 			string_data[i++] = s.c_str();
// 		}

		if (!ReportEvent(source, wType, category, tID, NULL, static_cast<WORD>(strings.size()), 0, string_data, NULL)) {
			return nscapi::protobuf::functions::set_response_bad(*response, "Could not report the event");
		} else {
			return nscapi::protobuf::functions::set_response_good(*response, "Message reported successfully");
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to parse command line: ", e);
		return nscapi::protobuf::functions::set_response_bad(*response, "Error");
	}
}
