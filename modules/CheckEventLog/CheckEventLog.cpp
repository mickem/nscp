/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <nsclient/nsclient_exception.hpp>

#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

#include "CheckEventLog.h"

#include <time.h>
#include <error/error.hpp>
#include <map>
#include <vector>
#include <winevt.h>

#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "filter.hpp"

#include <parsers/filter/cli_helper.hpp>
#include <buffer.hpp>
#include <handle.hpp>
#include <compat.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

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

	thread_.reset(new real_time_thread(get_core(), get_id()));
	if (!thread_) {
		NSC_LOG_ERROR_STD("Failed to create thread container");
		return false;
	}
	thread_->set_path(settings.alias().get_settings_path("real-time/filters"));

	settings.alias().add_path_to_settings()
		("Eventlog", "Section for the EventLog Checker (CheckEventLog.dll).")

		("real-time", "Real-time eventlog monitoring", "A set of options to configure the real time checks")

		("real-time/filters", sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, thread_, get_settings_proxy(), _1, _2)),
			"Real-time eventlog filters", "A set of filters to use in real-time mode",
			"FILTER DEFENITION", "For more configuration options add a dedicated section")
		;

	settings.alias().add_key_to_settings()
		("debug", sh::bool_key(&debug_, false),
			"Enable debugging", "Log more information when filtering (useful to detect issues with filters) not useful in production as it is a bit of a resource hog.")

		("lookup names", sh::bool_key(&lookup_names_, true),
			"Lookup eventlog names", "Lookup the names of eventlog files")

		("syntax", sh::string_key(&syntax_),
			"Default syntax", "Set this to use a specific syntax string for all commands (that don't specify one).")

		("buffer size", sh::int_key(&buffer_length_, 128 * 1024),
			"Default buffer size", "The size of the buffer to use when getting messages this affects the speed and maximum size of messages you can receive.")

		;

	settings.alias().add_key_to_settings("real-time")

		("enabled", sh::bool_fun_key(boost::bind(&real_time_thread::set_enabled, thread_, _1), false),
			"Enable realtime monitoring", "Spawns a background thread which detects issues and reports them back instantly.")

		("startup age", sh::string_fun_key(boost::bind(&real_time_thread::set_start_age, thread_, _1), "30m"),
			"Read old records at startup", "The initial age to scan when starting NSClient++")

		("log", sh::string_key(&thread_->logs_, "application,system"),
			"Logs to check", "Comma separated list of logs to check")

		("debug", sh::bool_key(&thread_->debug_, false),
			"Enable debugging", "Log missed records (useful to detect issues with filters) not useful in production as it is a bit of a resource hog.")

		;
	std::string filter_path = settings.alias().get_settings_path("real-time/filters");

	settings.register_all();
	settings.notify();

	thread_->filters_.add_samples(get_settings_proxy());
	thread_->filters_.add_missing(get_settings_proxy(), "default", "");

	if (mode == NSCAPI::normalStart) {

		nscapi::core_helper core(get_core(), get_id());
		BOOST_FOREACH(const nscapi::core_helper::storage_map::value_type &e, core.get_storage_strings("eventlog.bookmarks")) {
			bookmarks_.add(e.first, e.second);
		}

		if (!thread_->start())
			NSC_LOG_ERROR_STD("Failed to start collection thread");
	}
	return true;
}
bool CheckEventLog::unloadModule() {
	if (!thread_->stop())
		NSC_LOG_ERROR_STD("Failed to start collection thread");

	nscapi::core_helper core(get_core(), get_id());
 	BOOST_FOREACH(const bookmarks::map_type::value_type &v, bookmarks_.get_copy()) {
 		core.put_storage("eventlog.bookmarks", v.first, v.second, false, false);
 	}
	return true;
}

typedef hlp::buffer<BYTE, EVENTLOGRECORD*> eventlog_buffer;

inline std::time_t to_time_t_epoch(boost::posix_time::ptime t) {
	if (t == boost::date_time::neg_infin)
		return 0;
	else if (t == boost::date_time::pos_infin)
		return LONG_MAX;
	boost::posix_time::ptime start(boost::gregorian::date(1970, 1, 1));
	return (t - start).total_seconds();
}

inline long long parse_time(std::string time) {
	long long now = to_time_t_epoch(boost::posix_time::second_clock::universal_time());
	std::string::size_type p = time.find_first_not_of("-0123456789");
	if (p == std::string::npos)
		return now + boost::lexical_cast<long long>(time);
	long long value = boost::lexical_cast<long long>(time.substr(0, p));
	if ((time[p] == 's') || (time[p] == 'S'))
		return now + value;
	else if ((time[p] == 'm') || (time[p] == 'M'))
		return now + (value * 60);
	else if ((time[p] == 'h') || (time[p] == 'H'))
		return now + (value * 60 * 60);
	else if ((time[p] == 'd') || (time[p] == 'D'))
		return now + (value * 24 * 60 * 60);
	else if ((time[p] == 'w') || (time[p] == 'W'))
		return now + (value * 7 * 24 * 60 * 60);
	return now + value;
}

void check_legacy(const std::string &logfile, std::string &scan_range, const int truncate_message, eventlog_filter::filter &filter) {
	typedef eventlog_filter::filter filter_type;
	eventlog_buffer buffer(4096);
	HANDLE hLog = OpenEventLog(NULL, utf8::cvt<std::wstring>(logfile).c_str());
	if (hLog == NULL)
		throw nsclient::nsclient_exception("Could not open the '" + logfile + "' event log: " + error::lookup::last_error());
	long long stop_date;
	enum direction_type {
		direction_none, direction_forwards, direction_backwards
	};
	direction_type direction = direction_none;
	DWORD flags = EVENTLOG_SEQUENTIAL_READ;
	if ((scan_range.size() > 0) && (scan_range[0] == L'-')) {
		direction = direction_backwards;
		flags |= EVENTLOG_BACKWARDS_READ;
		stop_date = parse_time(scan_range);
	} else if (scan_range.size() > 0) {
		direction = direction_forwards;
		flags |= EVENTLOG_FORWARDS_READ;
		stop_date = parse_time(scan_range);
	} else {
		flags |= EVENTLOG_FORWARDS_READ;
	}

	DWORD dwRead, dwNeeded;
	bool is_scanning = true;
	while (is_scanning) {
		BOOL bStatus = ReadEventLog(hLog, flags, 0, buffer.get(), static_cast<DWORD>(buffer.size()), &dwRead, &dwNeeded);
		if (bStatus == FALSE) {
			DWORD err = GetLastError();
			if (err == ERROR_INSUFFICIENT_BUFFER) {
				buffer.resize(dwNeeded);
				if (!ReadEventLog(hLog, flags, 0, buffer.get(), static_cast<DWORD>(buffer.size()), &dwRead, &dwNeeded))
					throw nsclient::nsclient_exception("Error reading eventlog: " + error::lookup::last_error());
			} else if (err == ERROR_HANDLE_EOF) {
				is_scanning = false;
				break;
			} else {
				std::string error_msg = error::lookup::last_error(err);
				CloseEventLog(hLog);
				throw nsclient::nsclient_exception("Failed to read from event log: " + error_msg);
			}
		}
		__time64_t ltime;
		_time64(&ltime);

		EVENTLOGRECORD *pevlr = buffer.get();
		while (dwRead > 0) {
			EventLogRecord record(logfile, pevlr);
			if (direction == direction_backwards && record.written() < stop_date) {
				is_scanning = false;
				break;
			}
			if (direction == direction_forwards && record.written() > stop_date) {
				is_scanning = false;
				break;
			}
			modern_filter::match_result ret = filter.match(filter_type::object_type(new eventlog_filter::old_filter_obj(ltime, logfile, pevlr, truncate_message)));
			dwRead -= pevlr->Length;
			pevlr = reinterpret_cast<EVENTLOGRECORD*>((LPBYTE)pevlr + pevlr->Length);
		}
	}
	CloseEventLog(hLog);
}

void CheckEventLog::save_bookmark(const std::string bookmark, eventlog::api::EVT_HANDLE &hResults) {
	if (bookmark.empty()) {
		return;
	}
	eventlog::evt_handle hBookmark = eventlog::EvtCreateBookmark(NULL);
	if (!hBookmark) {
		NSC_LOG_ERROR("Failed to create bookmark: " + error::lookup::last_error());
		return;
	}
	if (!EvtUpdateBookmark(hBookmark, hResults)) {
		NSC_LOG_ERROR("Failed to create bookmark: " + error::lookup::last_error());
		return;
	}

	hlp::buffer<wchar_t, LPWSTR> buffer(4096);
	DWORD dwBufferSize = 0;
	DWORD dwPropertyCount = 0;

	if (!eventlog::EvtRender(NULL, hBookmark, eventlog::api::EvtRenderBookmark, static_cast<DWORD>(buffer.size()), buffer.get(), &dwBufferSize, &dwPropertyCount)) {
		DWORD status = GetLastError();
		if (status == ERROR_INSUFFICIENT_BUFFER) {
			buffer.resize(dwBufferSize);
			if (!eventlog::EvtRender(NULL, hBookmark, eventlog::api::EvtRenderBookmark, static_cast<DWORD>(buffer.size()), buffer.get(), &dwBufferSize, &dwPropertyCount)) {
				NSC_LOG_ERROR("Failed to save bookmark: " + error::lookup::last_error());
				return;
			}
		}
	}
	bookmarks_.add(bookmark, utf8::cvt<std::string>(buffer.get()));
}

void CheckEventLog::check_modern(const std::string &logfile, const std::string &scan_range, const int truncate_message, eventlog_filter::filter &filter, std::string bookmark) {
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
	} else {
		direction = direction_backwards;
		flags |= eventlog::api::EvtQueryReverseDirection;
		stop_date = parse_time("24h");
	}
	eventlog::evt_handle hResults = eventlog::EvtQuery(NULL, utf8::cvt<std::wstring>(logfile).c_str(), pwsQuery, flags);
	if (!hResults) {
		status = GetLastError();
		if (status == ERROR_EVT_CHANNEL_NOT_FOUND)
			throw nsclient::nsclient_exception("Channel " + logfile + " not found: " + error::lookup::last_error(status));
		else if (status == ERROR_EVT_INVALID_QUERY)
			throw nsclient::nsclient_exception("Failed to open " + logfile + ": " + error::lookup::last_error(status));
		else if (status != ERROR_SUCCESS)
			throw nsclient::nsclient_exception("Failed to open " + logfile + ": " + error::lookup::last_error(status));
	}

	eventlog::evt_handle hContext = eventlog::EvtCreateRenderContext(0, NULL, eventlog::api::EvtRenderContextSystem);
	if (!hContext)
		throw nsclient::nsclient_exception("EvtCreateRenderContext failed: " + error::lookup::last_error());

	if (!bookmark.empty()) {
		bookmarks::op_string xmlBm = bookmarks_.get(bookmark);
		if (xmlBm) {
			eventlog::evt_handle hBookmark = eventlog::EvtCreateBookmark(utf8::cvt<std::wstring>(*xmlBm).c_str());
			if (!hBookmark) {
				NSC_LOG_ERROR("Failed to create bookmark: " + error::lookup::last_error());
			} else {
				if (!eventlog::EvtSeek(hResults, 1, hBookmark, 0, eventlog::api::EvtSeekRelativeToBookmark)) {
					NSC_LOG_ERROR("Failed to lookup bookmark: " + error::lookup::last_error());
				}
			}
		}
	}
	while (true) {
		DWORD status = ERROR_SUCCESS;
		hlp::buffer<eventlog::api::EVT_HANDLE> hEvents(batch_size);
		DWORD dwReturned = 0;

		__time64_t ltime;
		_time64(&ltime);

		while (true) {
			if (!eventlog::EvtNext(hResults, batch_size, hEvents, 100, 0, &dwReturned)) {
				status = GetLastError();
				if (status == ERROR_NO_MORE_ITEMS || status == ERROR_TIMEOUT) {
					if (!bookmark.empty()) {
						NSC_LOG_ERROR("Cannot update bookmarks for empty reads yet");
						//save_bookmark(bookmark, hResults);
					}
					return;
				}
				else if (status != ERROR_SUCCESS)
					throw nsclient::nsclient_exception("EvtNext failed: " + error::lookup::last_error(status));
			}
			for (DWORD i = 0; i < dwReturned; i++) {
				try {
					filter_type::object_type item(new eventlog_filter::new_filter_obj(ltime, logfile, hEvents[i], hContext, truncate_message));
					if (direction == direction_backwards && item->get_written() < stop_date) {
						if (dwReturned > 0) {
							save_bookmark(bookmark, hEvents[dwReturned - 1]);
						}
						for (; i < dwReturned; i++)
							eventlog::EvtClose(hEvents[i]);
						return;
					}
					if (direction == direction_forwards && item->get_written() > stop_date) {
						if (dwReturned > 0) {
							save_bookmark(bookmark, hEvents[dwReturned - 1]);
						}
						for (; i < dwReturned; i++)
							eventlog::EvtClose(hEvents[i]);
						return;
					}
					modern_filter::match_result ret = filter.match(item);
				} catch (const nsclient::nsclient_exception &e) {
					for (; i < dwReturned; i++)
						eventlog::EvtClose(hEvents[i]);
					NSC_LOG_ERROR("Failed to describe event: " + e.reason());
				} catch (...) {
					for (; i < dwReturned; i++)
						eventlog::EvtClose(hEvents[i]);
					NSC_LOG_ERROR("Failed to describe event");
				}
			}
		}
	}
}

void log_args(const Plugin::QueryRequestMessage::Request &request) {
	std::stringstream ss;
	for (int i = 0; i < request.arguments_size(); i++) {
		if (i > 0)
			ss << " ";
		ss << request.arguments(i);
	}
	NSC_DEBUG_MSG("Created command: " + ss.str());
}

void CheckEventLog::CheckEventLog_(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	boost::program_options::options_description desc;
	bool debug = false;
	std::string filter, syntax, scan_range, top_syntax;
	std::vector<std::string> times;
	nscapi::program_options::add_help(desc);
	bool unique = false;

	compat::addAllNumeric(desc);
	compat::addOldNumeric(desc);

	desc.add_options()
		("filter", po::value<std::string>(&filter), "The filter to use.")
		("file", po::value<std::vector<std::string>>(&times), "The file to check")
		("debug", po::value<bool>(&debug)->implicit_value("true"), "The file to check")
		("truncate", po::value<std::string>(), "Deprecated and has no meaning")
		("descriptions", po::value<bool>()->implicit_value("true"), "Deprecated and has no meaning")
		("unique", po::value<bool>(&unique)->implicit_value("true"), "")
		("syntax", po::value<std::string>(&syntax)->default_value("%source%, %strings%"), "The syntax string")
		("top-syntax", po::value<std::string>(&top_syntax)->default_value("${list}"), "The top level syntax string")
		("scan-range", po::value<std::string>(&scan_range), "TODO")
		;

	boost::program_options::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	std::string warn, crit;

	request.clear_arguments();
	compat::matchFirstNumeric(vm, "count", "count", warn, crit);
	compat::matchFirstOldNumeric(vm, "count", warn, crit);

	compat::inline_addarg(request, warn);
	compat::inline_addarg(request, crit);
	compat::inline_addarg(request, "scan-range=", scan_range);

	BOOST_FOREACH(const std::string &t, times) {
		request.add_arguments("file=" + t);
	}
	if (debug)
		request.add_arguments("debug");
	if (unique)
		request.add_arguments("unique");
	if (!filter.empty()) {
		if (eventlog::api::supports_modern()) {
			boost::replace_all(filter, "strings", "message");
			boost::replace_all(filter, "generated", "written");
			boost::replace_all(filter, "severity", "level");
		}
		request.add_arguments("filter=" + filter);
	}
	boost::replace_all(syntax, "%message%", "${message}");
	boost::replace_all(syntax, "%source%", "${source}");
	boost::replace_all(syntax, "%computer%", "${computer}");
	boost::replace_all(syntax, "%written%", "${written}");
	boost::replace_all(syntax, "%type%", "${type}");
	boost::replace_all(syntax, "%category%", "${category}");
	boost::replace_all(syntax, "%facility%", "${facility}");
	boost::replace_all(syntax, "%qualifier%", "${qualifier}");
	boost::replace_all(syntax, "%customer%", "${customer}");
	if (eventlog::api::supports_modern()) {
		boost::replace_all(syntax, "%strings%", "${message}");
		boost::replace_all(syntax, "%generated%", "${written}");
		boost::replace_all(syntax, "%severity%", "${level}");
	} else {
		boost::replace_all(syntax, "%strings%", "${strings}");
		boost::replace_all(syntax, "%generated%", "${generated}");
		boost::replace_all(syntax, "%severity%", "${severity}");
	}
	boost::replace_all(syntax, "%count%", "${count}");
	boost::replace_all(syntax, "%level%", "${level}");
	boost::replace_all(syntax, "%log%", "${file}");
	boost::replace_all(syntax, "%file%", "${file}");
	boost::replace_all(syntax, "%id%", "${id}");
	boost::replace_all(syntax, "%user%", "${user}");
	request.add_arguments("detail-syntax=" + syntax);
	request.add_arguments("top-syntax=" + top_syntax);
	log_args(request);
	check_eventlog(request, response);
}

void CheckEventLog::check_eventlog(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	typedef eventlog_filter::filter filter_type;
	modern_filter::data_container data;
	modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;
	std::string scan_range;
	std::string bookmark;
	bool unique = false;
	int truncate_message = 0;

	filter_type filter;
	filter_helper.set_default_perf_config("level(ignored:true)");


	filter_helper.add_filter_option("level in ('warning', 'error', 'critical')");
	filter_helper.add_warn_option("level = 'warning'", "problem_count > 0");
	filter_helper.add_crit_option("level in ('error', 'critical')");
	filter_helper.add_options(filter.get_filter_syntax(), "ok");
	filter_helper.add_index("");
	filter_helper.add_syntax("${status}: ${count} message(s) ${problem_list}", "${file} ${source} (${message})", "${file}_${source}", "%(status): No entries found", "%(status): Event log seems fine");
	filter_helper.get_desc().add_options()
		("file", po::value<std::vector<std::string> >(&file_list), "File to read (can be specified multiple times to check multiple files.\nNotice that specifying multiple files will create an aggregate set you will not check each file individually."
			"In other words if one file contains an error the entire check will result in error.")
		("log", po::value<std::vector<std::string>>(&file_list), "Same as file")
		("scan-range", po::value<std::string>(&scan_range), "Date range to scan.\nA negative value scans backward (historical events) and a positive value scans forwards (future events). This is the approximate dates to search through this speeds up searching a lot but there is no guarantee messages are ordered.")
		("truncate-message", po::value<int>(&truncate_message), "Maximum length of message for each event log message text.")
		("unique", po::value<bool>(&unique)->implicit_value("true"), "Shorthand for setting default unique index: ${log}-${source}-${id}.")
		("bookmark", po::value<std::string>(&bookmark)->implicit_value("auto"), "Use bookmarks to only look for messages since last check (with the same bookmark name). If you set this to auto or leave it empty the bookmark name will be derived from your logs, filters, warn and crit.")
		;
	if (!filter_helper.parse_options())
		return;

	if (scan_range.empty())
		scan_range = "-24h";

	if (unique) {
		filter_helper.set_default_index("${log}-${source}-${id}");
	}
	if (file_list.empty()) {
		file_list.push_back("Application");
		file_list.push_back("System");
	}
	std::string bookmark_prefix = "auto,log[", bookmark_suffix;
	if (bookmark == "auto") {
		bookmark_suffix += "],filters[";
		BOOST_FOREACH(const std::string &file, filter_helper.data.filter_string) {
			bookmark_suffix += "," + file;
		}
		bookmark_suffix += "],warn[";
		BOOST_FOREACH(const std::string &file, filter_helper.data.warn_string) {
			bookmark_suffix += "," + file;
		}
		bookmark_suffix += "],crit[";
		BOOST_FOREACH(const std::string &file, filter_helper.data.crit_string) {
			bookmark_suffix += "," + file;
		}
		bookmark_suffix += "]";
	}

	if (!filter_helper.build_filter(filter))
		return;

	BOOST_FOREACH(const std::string &file, file_list) {
		if (!bookmark_suffix.empty()) {
			bookmark = bookmark_prefix + file + bookmark_suffix;
		}
		std::string name = file;
		if (lookup_names_) {
			name = eventlog_wrapper::find_eventlog_name(name);
			if (file != name) {
				NSC_DEBUG_MSG_STD("Opening alternative log: " + utf8::cvt<std::string>(name));
			}
		}
		if (eventlog::api::supports_modern())
			check_modern(name, scan_range, truncate_message, filter, bookmark);
		else
			check_legacy(name, scan_range, truncate_message, filter);
	}
	filter_helper.post_process(filter);
}

bool CheckEventLog::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &) {
	std::string command = request.command();
	if (command == "eventlog" && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (target_mode == NSCAPI::target_module && request.arguments_size() > 0)
		command = request.arguments(0);
	else if (command.empty() && target_mode == NSCAPI::target_module)
		command = "help";
	if (command == "insert-message" || command == "insert") {
		insert_eventlog(request, response);
		return true;
	} else if (command == "list-providers" || command == "list") {
		list_providers(request, response);
		return true;
	} else if (command == "add") {
		add_filter(request, response);
		return true;
	} else if (target_mode == NSCAPI::target_module) {
		nscapi::protobuf::functions::set_response_good(*response, "Usage: nscp eventlog [list|insert|add] --help");
		return true;
	}
	return false;
}

void CheckEventLog::list_providers(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	try {
		namespace po = boost::program_options;
		po::variables_map vm;
		po::options_description desc("Allowed options");
		bool help = false, channels = false, publishers = false, tasks = false, keywords = false, all = false;
		std::string publisher;
		desc.add_options()
			("help,h", po::bool_switch(&help), "Show help screen")
			("channels", po::bool_switch(&channels), "List channels (logs)")
			("publishers", po::bool_switch(&publishers), "List publisher (sources)")
			("tasks", po::bool_switch(&tasks), "List tasks")
			("keywords", po::bool_switch(&keywords), "List keywords")
			("publisher", po::value(&publisher), "Only list keywords and tasks for a given publisher")
			("all", po::bool_switch(&all), "List everything")
			;


		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);
		po::parsed_options parsed = cmd.allow_unregistered().run();
		po::store(parsed, vm);
		po::notify(vm);

		if (!channels && !publishers && !tasks && !keywords && !all)
			help = true;
		if (help) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return;
		}

		std::stringstream ss;

		if (channels || all) {
			if (all)
				ss << "Channels:\n";
			eventlog::evt_handle hProviders;
			hlp::buffer<WCHAR, LPWSTR> buffer(1024);
			DWORD dwBufferSize = 0;
			DWORD status = ERROR_SUCCESS;
			hProviders = eventlog::EvtOpenChannelEnum(NULL, 0);
			if (!hProviders) {
				NSC_LOG_ERROR("EvtOpenChannelEnum failed: ", error::lookup::last_error());
				return;
			}
			while (true) {
				if (!eventlog::EvtNextChannelPath(hProviders, buffer.size(), buffer.get(), &dwBufferSize)) {
					status = GetLastError();
					if (ERROR_NO_MORE_ITEMS == status)
						break;
					else if (ERROR_INSUFFICIENT_BUFFER == status) {
						buffer.resize(dwBufferSize);
						if (!eventlog::EvtNextChannelPath(hProviders, buffer.size(), buffer.get(), &dwBufferSize))
							throw nsclient::nsclient_exception("EvtNextChannelPath failed: " + error::lookup::last_error());
					} else if (status != ERROR_SUCCESS)
						throw nsclient::nsclient_exception("EvtNextChannelPath failed: " + error::lookup::last_error(status));
				}
				if (channels || all) {
					ss << utf8::cvt<std::string>(buffer.get()) << "\n";

				} else {
					eventlog::evt_handle hProvider;

					hProvider = eventlog::EvtOpenPublisherMetadata(NULL, buffer.get(), NULL, 0, 0);
					if (!hProvider) {
						ss << "Failed to open provider: " << error::lookup::last_error() << "\n";
						continue;
					}

					bool match = false;
					if (tasks) {
						eventlog::eventlog_table tbl = eventlog::fetch_table(hProvider, eventlog::api::EvtPublisherMetadataTasks, eventlog::api::EvtPublisherMetadataTaskValue, eventlog::api::EvtPublisherMetadataTaskName);
						if (tbl.size() > 0) {
							ss << utf8::cvt<std::string>(buffer.get()) << "\n";
							match = true;
							ss << "Tasks:\n";
							BOOST_FOREACH(const eventlog::eventlog_table::value_type &v, tbl) {
								ss << " * " << v.second << "\n";
							}
						}
					}
					if (keywords) {
						eventlog::eventlog_table tbl = eventlog::fetch_table(hProvider, eventlog::api::EvtPublisherMetadataKeywords, eventlog::api::EvtPublisherMetadataKeywordValue, eventlog::api::EvtPublisherMetadataKeywordName);
						if (tbl.size() > 0) {
							if (!match)
								ss << utf8::cvt<std::string>(buffer.get()) << "\n";
							match = true;
							ss << "Keywords:\n";
							BOOST_FOREACH(const eventlog::eventlog_table::value_type &v, tbl) {
								ss << " * " << v.second << "\n";
							}
						}
					}
				}
			}

		}

		if (publishers || tasks || keywords || all) {
			if (all)
				ss << "\n\nPublisher:\n";
			if (!publisher.empty()) {
				eventlog::evt_handle hProvider;

				hProvider = eventlog::EvtOpenPublisherMetadata(NULL, utf8::cvt<std::wstring>(publisher).c_str(), NULL, 0, 0);
				if (!hProvider) {
					ss << "Failed to open provider: " << error::lookup::last_error() << "\n";
				}

				bool match = false;
				if (tasks) {
					eventlog::eventlog_table tbl = eventlog::fetch_table(hProvider, eventlog::api::EvtPublisherMetadataTasks, eventlog::api::EvtPublisherMetadataTaskValue, eventlog::api::EvtPublisherMetadataTaskName);
					if (tbl.size() > 0) {
						ss << publisher << "\n";
						match = true;
						ss << "Tasks:\n";
						BOOST_FOREACH(const eventlog::eventlog_table::value_type &v, tbl) {
							ss << " * " << v.second << " (" << v.first << ")\n";
						}
					}
				}
				if (keywords) {
					eventlog::eventlog_table tbl = eventlog::fetch_table(hProvider, eventlog::api::EvtPublisherMetadataKeywords, eventlog::api::EvtPublisherMetadataKeywordValue, eventlog::api::EvtPublisherMetadataKeywordName);
					if (tbl.size() > 0) {
						if (!match)
							ss << publisher << "\n";
						match = true;
						ss << "Keywords:\n";
						BOOST_FOREACH(const eventlog::eventlog_table::value_type &v, tbl) {
							ss << " * " << v.second << " (" << v.first << ")\n";
						}
					}
				}
			} else {
				eventlog::evt_handle hProviders;
				hlp::buffer<WCHAR, LPWSTR> buffer(1024);
				DWORD dwBufferSize = 0;
				DWORD status = ERROR_SUCCESS;
				hProviders = eventlog::EvtOpenPublisherEnum(NULL, 0);
				if (!hProviders) {
					NSC_LOG_ERROR("EvtOpenPublisherEnum failed: ", error::lookup::last_error());
					return;
				}
				while (true) {
					if (!eventlog::EvtNextPublisherId(hProviders, buffer.size(), buffer.get(), &dwBufferSize)) {
						status = GetLastError();
						if (ERROR_NO_MORE_ITEMS == status)
							break;
						else if (ERROR_INSUFFICIENT_BUFFER == status) {
							buffer.resize(dwBufferSize);
							if (!eventlog::EvtNextPublisherId(hProviders, buffer.size(), buffer.get(), &dwBufferSize))
								throw nsclient::nsclient_exception("EvtNextChannelPath failed: " + error::lookup::last_error());
						} else if (status != ERROR_SUCCESS)
							throw nsclient::nsclient_exception("EvtNextChannelPath failed: " + error::lookup::last_error(status));
					}
					if (channels || all) {
						ss << utf8::cvt<std::string>(buffer.get()) << "\n";

					} else {
						eventlog::evt_handle hProvider;

						hProvider = eventlog::EvtOpenPublisherMetadata(NULL, buffer.get(), NULL, 0, 0);
						if (!hProvider) {
							ss << "Failed to open provider: " << error::lookup::last_error() << "\n";
							continue;
						}

						bool match = false;
						if (tasks) {
							eventlog::eventlog_table tbl = eventlog::fetch_table(hProvider, eventlog::api::EvtPublisherMetadataTasks, eventlog::api::EvtPublisherMetadataTaskValue, eventlog::api::EvtPublisherMetadataTaskName);
							if (tbl.size() > 0) {
								ss << utf8::cvt<std::string>(buffer.get()) << "\n";
								match = true;
								ss << "Tasks:\n";
								BOOST_FOREACH(const eventlog::eventlog_table::value_type &v, tbl) {
									ss << " * " << v.second << " (" << v.first << ")\n";
								}
							}
						}
						if (keywords) {
							eventlog::eventlog_table tbl = eventlog::fetch_table(hProvider, eventlog::api::EvtPublisherMetadataKeywords, eventlog::api::EvtPublisherMetadataKeywordValue, eventlog::api::EvtPublisherMetadataKeywordName);
							if (tbl.size() > 0) {
								if (!match)
									ss << utf8::cvt<std::string>(buffer.get()) << "\n";
								match = true;
								ss << "Keywords:\n";
								BOOST_FOREACH(const eventlog::eventlog_table::value_type &v, tbl) {
									ss << " * " << v.second << " (" << v.first << ")\n";
								}
							}
						}
					}
				}
			}
		}

		nscapi::protobuf::functions::set_response_good(*response, ss.str());

	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to parse command line: ", e);
		return nscapi::protobuf::functions::set_response_bad(*response, "Error");
	}
}

void CheckEventLog::add_filter(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	try {
		namespace po = boost::program_options;
		po::variables_map vm;
		po::options_description desc("Allowed options");
		bool help = false;
		std::string alias, filter, target, log;
		desc.add_options()
			("help,h", po::bool_switch(&help), "Show help screen")
			("alias", po::value(&alias), "The alias of the new filter")
			("filter", po::value(&filter)->default_value("level = 'error'"), "The filter of the new filter to add")
			("target", po::value(&target)->default_value("log"), "Where messages are sent")
			("log", po::value(&log)->default_value("application"), "The log file to subscribe to")
			;


		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);
		po::parsed_options parsed = cmd.allow_unregistered().run();
		po::store(parsed, vm);
		po::notify(vm);

		if (alias.empty())
			help = true;
		if (help) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return;
		}


		nscapi::protobuf::functions::settings_query s(get_id());
		s.set("/settings/eventlog/real-time", "enabled", "true");
		s.set("/settings/eventlog/real-time/filters/" + alias, "filter", filter);
		s.set("/settings/eventlog/real-time/filters/" + alias, "target", target);
		s.set("/settings/eventlog/real-time/filters/" + alias, "log", log);
		s.set("/modules", "CheckEventLog", "enabled");
		s.save();
		get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return;
		}

		nscapi::protobuf::functions::set_response_good(*response, "FIlter " + alias + " added");

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
			("help,h", "Show help screen")
			("source,s", po::wvalue<std::wstring>(&source_name)->default_value(L"Application Error"), "source to use")
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


		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		boost::program_options::variables_map vm;
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help")) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return;
		}

		event_source source(source_name);
		WORD wType = EventLogRecord::translateType(type);
		WORD wSeverity = EventLogRecord::translateSeverity(severity);
		DWORD tID = (wEventID & 0xffff) | ((facility & 0xfff) << 16) | ((customer & 0x1) << 29) | ((wSeverity & 0x3) << 30);
		LPCWSTR *string_data = new LPCWSTR[strings.size() + 1];
		int i = 0;
		// TODO: FIxme this is broken!
		BOOST_FOREACH(const std::wstring &s, strings) {
			string_data[i++] = s.c_str();
		}
		string_data[i++] = 0;

		if (!ReportEvent(source, wType, category, tID, NULL, static_cast<WORD>(strings.size()), 0, string_data, NULL)) {
			delete[] string_data;
			return nscapi::protobuf::functions::set_response_bad(*response, "Could not report the event: " + error::lookup::last_error());
		} else {
			delete[] string_data;
			return nscapi::protobuf::functions::set_response_good(*response, "Message reported successfully");
		}
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to parse command line: ", e);
		return nscapi::protobuf::functions::set_response_bad(*response, "Error");
	}
}
