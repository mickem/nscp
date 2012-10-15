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

#ifndef WIN32
#include <poll.h>
#include <sys/inotify.h>
#endif

#include <map>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/filesystem.hpp>

#include <parsers/expression/expression.hpp>

#include <time.h>
#include <utils.h>
#include <error.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <simple_timer.hpp>
#include <settings/client/settings_client.hpp>

#include "CheckLogFile.h"
#include "filter.hpp"
#include "filters.hpp"

namespace sh = nscapi::settings_helper;

CheckLogFile::CheckLogFile() {
}
CheckLogFile::~CheckLogFile() {
}
struct parse_exception {
	parse_exception(std::wstring) {}
};


bool CheckLogFile::loadModule() {
	return false;
}


void real_time_thread::process_timeout(const filters::filter_config_object &object) {
	std::wstring response;
	std::wstring command = object.alias;
	NSC_LOG_ERROR(_T("Processing timeout: ") + object.alias);
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, NSCAPI::returnOK, object.empty_msg, _T(""), response)) {
		NSC_LOG_ERROR(_T("Failed to submit result: ") + response);
	}
}

void real_time_thread::process_object(filters::filter_config_object &object) {
	std::wstring response;
	int severity = object.severity;
	std::wstring command = object.alias;
	if (severity != -1) {
		object.filter.returnCode = severity;
	} else {
		object.filter.returnCode = NSCAPI::returnOK;
	}
	object.filter.reset();

	bool matched = false;
	NSC_LOG_ERROR(_T("Processing object: ") + object.alias);
	BOOST_FOREACH(filters::file_container &c, object.files) {
		boost::uintmax_t sz = boost::filesystem::file_size(c.file);
		std::string fname = utf8::cvt<std::string>(c.file);
		std::ifstream file(fname.c_str());
		if (file.is_open()) {
			std::string line;
			object.filter.summary.filename = fname;
			if (sz == c.size) {
				continue;
			} else if (sz > c.size) {
				file.seekg(c.size);
			}
			while (file.good()) {
				std::getline(file,line, '\n');
				if (!object.column_split.empty()) {
					std::list<std::string> chunks = strEx::s::splitEx(line, utf8::cvt<std::string>(object.column_split));
					boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(object.filter.summary.filename, line, chunks, object.filter.summary.match_count));
					boost::tuple<bool,bool> ret = object.filter.match(record);
					if (ret.get<0>()) {
						matched = true;
						if (ret.get<1>()) {
							break;
						}
					}
				}
			}
			file.close();
		} else {
			NSC_LOG_ERROR(_T("Failed to open file: ") + c.file);
		}
	}
	if (!matched) {
		return;
	}

	std::string message;
	if (object.filter.message.empty())
		message = "Nothing matched";
	else
		message = object.filter.message;
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, object.filter.returnCode, utf8::cvt<std::wstring>(message), _T(""), response)) {
		NSC_LOG_ERROR(_T("Failed to submit '") + utf8::cvt<std::wstring>(message) + _T("' ") + object.alias + _T(": ") + response);
	}
}

void real_time_thread::thread_proc() {

	std::list<filters::filter_config_object> filters;
	std::list<std::wstring> logs;

	BOOST_FOREACH(filters::filter_config_object object, filters_.get_object_list()) {
		logfile_filter::filter filter;
		std::string message;
		if (!object.boot(message)) {
			NSC_LOG_ERROR(_T("Failed to load ") + object.alias + _T(": ") + utf8::cvt<std::wstring>(message));
			continue;
		}
		BOOST_FOREACH(const filters::file_container &fc, object.files) {
			boost::filesystem::wpath path = fc.file;
#ifdef WIN32
			if (boost::filesystem::is_directory(path)) {
				logs.push_back(path.string());
			} else {
				path = path.remove_filename();
				if (boost::filesystem::is_directory(path)) {
					logs.push_back(path.string());
				} else {
					NSC_LOG_ERROR(_T("Failed to find folder for ") + object.alias + _T(": ") + fc.file);
					continue;
				}
			}
#else
			if (boost::filesystem::is_regular(path)) {
				logs.push_back(path.string());
			} else {
				NSC_LOG_ERROR(_T("Failed to find folder for ") + object.alias + _T(": ") + fc.file);
				continue;
			}
#endif
		}
		filters.push_back(object);
	}

	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD(_T("Scanning folders: ") + strEx::joinEx(logs, _T(", ")));
	std::vector<std::wstring> files_list(logs.begin(), logs.end());
#ifdef WIN32
	HANDLE *handles = new HANDLE[1+logs.size()];
	handles[0] = stop_event_;
	for (int i=0;i<files_list.size();i++) {
		NSC_DEBUG_MSG_STD(_T("Adding folder: ") + files_list[i]);
		handles[i+1] = FindFirstChangeNotification(files_list[i].c_str(), TRUE, FILE_NOTIFY_CHANGE_SIZE);
	}
#else

	struct pollfd pollfds[2] = { { inotify_init(), POLLIN|POLLPRI, 0}, { stop_event_[0], POLLIN, 0}};

	int *wds = new int[logs.size()];
	for (int i=0;i<files_list.size();i++) {
		NSC_DEBUG_MSG_STD(_T("Adding folder: ") + files_list[i]);
		wds[i] = inotify_add_watch(pollfds[0].fd, utf8::cvt<std::string>(files_list[i]).c_str(), IN_MODIFY);
	}

#endif

	boost::posix_time::ptime current_time;
	BOOST_FOREACH(filters::filter_config_object &object, filters) {
		object.touch(current_time);
	}
	while (true) {
		bool first = true;
		boost::posix_time::ptime minNext;
		BOOST_FOREACH(const filters::filter_config_object &object, filters) {
			NSC_DEBUG_MSG_STD(_T("Getting next from: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			if (object.max_age && (first || object.next_ok_ < minNext) ) {
				first = false;
				minNext = object.next_ok_;
			}
		}

		boost::posix_time::time_duration dur;
		if (first) {
			NSC_DEBUG_MSG(_T("Next miss time is in: no timeout specified"));
		} else {
			dur = minNext - boost::posix_time::ptime();
			NSC_DEBUG_MSG(_T("Next miss time is in: ") + strEx::itos(dur.total_seconds()) + _T("s"));
		}

#ifdef WIN32
		DWORD dwWaitTime = INFINITE;
		if (!first)
			dwWaitTime = dur.total_milliseconds();
		DWORD dwWaitReason = WaitForMultipleObjects(logs.size()+1, handles, FALSE, dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			// we take care of this below...
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete [] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + files_list.size())) {
			FindNextChangeNotification(handles[dwWaitReason-WAIT_OBJECT_0]);
		}
#else

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

		int timeout = 1000*60;
		if (!first)
			timeout = dur.total_milliseconds();
		char buffer[BUF_LEN];
		int length = poll(pollfds, 2, timeout);
		if( !length )
		{
			continue;
		}
		else if( length < 0 )
		{
			NSC_LOG_ERROR(_T("read failed!"));
			continue;
		}
		else if (pollfds[1].revents != 0) {
			return;
		} else if (pollfds[0].revents != 0) {
			length = read(pollfds[0].fd, buffer, BUF_LEN);  
			for (int j=0;j<length;) {
				struct inotify_event * event = (struct inotify_event *) &buffer[j];
				std::wstring wstr = utf8::cvt<std::wstring>( event->name);
				j += EVENT_SIZE + event->len;
			}
		} else {
			NSC_LOG_ERROR(_T("Strange, please report this..."));
		}
#endif

		current_time = boost::posix_time::ptime();

		BOOST_FOREACH(filters::filter_config_object &object, filters) {
 			if (object.has_changed()) {
 				process_object(object);
				object.touch(current_time);
 			}
		}

		//current_time = boost::posix_time::ptime() + boost::posix_time::seconds(1);
		BOOST_FOREACH(filters::filter_config_object &object, filters) {
			if (object.max_age && object.next_ok_ < current_time) {
				process_timeout(object);
				object.touch(current_time);
			} else {
				NSC_DEBUG_MSG_STD(_T("missing: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			}
		}

	}

#ifdef WIN32
	delete [] handles;
#else
	for (int i=0;i<files_list.size();i++) {
		inotify_rm_watch(pollfds[0].fd, wds[i]);
	}
	close(pollfds[0].fd);
	//close(pollfds[1].fd);
#endif
	return;
}


bool real_time_thread::start() {
	if (!enabled_)
		return true;
#ifdef WIN32
 	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("EventLogShutdown"));
#else
	pipe(stop_event_);
#endif
	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&real_time_thread::thread_proc, this)));
	return true;
}
bool real_time_thread::stop() {
#ifdef WIN32
 	SetEvent(stop_event_);
#else
	write(stop_event_[1], "EXIT", 4);
#endif
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


bool CheckLogFile::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		register_command(_T("CheckLogFile"), _T("Check for errors in log file or generic pattern matching in text files"));
		register_command(_T("check_logfile"), _T("Check for errors in log file or generic pattern matching in text files"));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, _T("logfile"));
		
		thread_.filters_path_ = settings.alias().get_settings_path(_T("real-time/checks"));


		settings.alias().add_path_to_settings()
			(_T("LOG FILE SECTION"), _T("Section for log file checker"))

			(_T("real-time"), _T("CONFIGURE REALTIME CHECKING"), _T("A set of options to configure the real time checks"))

			(_T("real-time/checks"), sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, &thread_, get_settings_proxy(), _1, _2)),  
			_T("REALTIME FILTERS"), _T("A set of filters to use in real-time mode"))
			;

		settings.alias().add_key_to_settings()
			(_T("debug"), sh::bool_key(&debug_, false),
			_T("DEBUG"), _T("Log more information to help diagnose errors and configuration problems."))

			(_T("syntax"), sh::wstring_key(&syntax_),
			_T("SYNTAX"), _T("Set the default syntax to use"))

			;

		settings.alias().add_key_to_settings(_T("real-time"))

			(_T("enabled"), sh::bool_fun_key<bool>(boost::bind(&real_time_thread::set_enabled, &thread_, _1), false),
			_T("REAL TIME CHECKING"), _T("Spawns a backgrounnd thread which waits for file changes."))

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
bool CheckLogFile::unloadModule() {
	if (!thread_.stop())
		NSC_LOG_ERROR_STD(_T("Failed to stop thread"));
	return true;
}

bool CheckLogFile::hasCommandHandler() {
	return true;
}
bool CheckLogFile::hasMessageHandler() {
	return false;
}


namespace po = boost::program_options;

std::vector<po::option> option_parser(std::vector<std::string> &args) {
	std::vector<po::option> result;
	BOOST_FOREACH(const std::string &s, args) {
		po::option opt;
		std::string::size_type pos = s.find('=');
		if (pos == std::string::npos) {
			opt.string_key = s;
		} else {
			opt.string_key = s.substr(0, pos);
			opt.value.push_back(s.substr(pos+1));
		}
		//opt.original_tokens.push_back(*i);
		result.push_back(opt);
	}
	args.clear();
	return result;
}



NSCAPI::nagiosReturn CheckLogFile::handleCommand(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &message, std::string &perf) {
	if (command != "check_logfile" && command != "checklogfile")
		return NSCAPI::returnIgnored;
	simple_timer time;

	std::list<std::wstring> files;
	std::string regexp;
	std::string column_split;
	std::string line_split;
	std::string filter_string, warn_string, crit_string, ok_string;
	std::string syntax_top, syntax_detail;
	std::vector<std::string> file_list;
	std::string files_string;
	std::string mode;
	bool debug;
	try {

		bool help;
		po::options_description desc("Allowed options");
		desc.add_options()
			("help", po::bool_switch(&help),							"Show help screen")
			("debug", po::bool_switch(&debug),							"Show more information to help configure filters")
			("regexp", po::value<std::string>(&regexp),					"Lookup a numeric value in the PDH index table")
			("split", po::value<std::string>(&column_split),			"Lookup a string value in the PDH index table")
			("line-split", po::value<std::string>(&line_split)->default_value("\\n"), 
																		"Lookup a string value in the PDH index table")
			("column-split", po::value<std::string>(&column_split),		"Expand a counter path contaning wildcards into corresponding objects (for instance --expand-path \\System\\*)")
			("filter", po::value<std::string>(&filter_string),			"Check that performance counters are working")
			("warn", po::value<std::string>(&warn_string),				"Filter which generates a warning state")
			("crit", po::value<std::string>(&crit_string),				"Filter which generates a critical state")
			("warning", po::value<std::string>(&warn_string),			"Filter which generates a warning state")
			("critical", po::value<std::string>(&crit_string),			"Filter which generates a critical state")
			("ok", po::value<std::string>(&ok_string),					"Filter which generates an ok state")
			("top-syntax", po::value<std::string>(&syntax_top)->default_value("${file}: ${count} (${messages})"), 
																		"Top level syntax")
			("detail-syntax", po::value<std::string>(&syntax_detail)->default_value("${column1}, "), 
																		"Detail level syntax")
			("file", po::value<std::vector<std::string> >(&file_list),	"List counters and/or instances")
			("files", po::value<std::string>(&files_string),			"List/check all counters not configured counter")
			("mode", po::value<std::string>(&mode),						"Mode of operation: count (count all critical/warning lines), find (find first critical/warning line)")
			;
		boost::program_options::variables_map vm;
		std::vector<std::string> a(arguments.begin(), arguments.end());
		po::command_line_parser cmd(a);
		cmd.options(desc);
		cmd.extra_style_parser(option_parser);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);

		if (help || (filter_string.empty() && file_list.empty())) {
			std::stringstream ss;
			ss << "check_logfile syntax:" << std::endl;
			ss << desc;
			message = ss.str();
			return NSCAPI::returnUNKNOWN;
		}

		logfile_filter::filter filter;
		if (!filter.build_syntax(syntax_top, syntax_detail, message)) {
			return NSCAPI::returnUNKNOWN;
		}
		filter.build_engines(filter_string, ok_string, warn_string, crit_string);

		if (!column_split.empty()) {
			strEx::replace(column_split, "\\t", "\t");
			strEx::replace(column_split, "\\n", "\n");
		}

		if (!filter.validate(message)) {
			return NSCAPI::returnUNKNOWN;
		}

		NSC_DEBUG_MSG_STD(_T("Boot time: ") + strEx::itos(time.stop()));

		BOOST_FOREACH(const std::string &filename, file_list) {
			std::ifstream file(filename.c_str());
			if (file.is_open()) {
				std::string line;
				filter.summary.filename = filename;
				while (file.good()) {
					std::getline(file,line, '\n');
					if (!column_split.empty()) {
						std::list<std::string> chunks = strEx::s::splitEx(line, column_split);
						boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(filename, line, chunks, filter.summary.match_count));
						boost::tuple<bool,bool> ret = filter.match(record);
						if (ret.get<1>()) {
							break;
						}
					}
				}
				file.close();
			} else {
				message = "Failed to open file: " + filename;
				return NSCAPI::returnUNKNOWN;
			}
		}
		NSC_DEBUG_MSG_STD(_T("Evaluation time: ") + strEx::itos(time.stop()));

		if (filter.message.empty())
			message = "Nothing matched";
		else
			message = filter.message;
		return filter.returnCode;
	} catch (checkHolders::parse_exception e) {
		message = utf8::cvt<std::string>(e.getMessage());
		return NSCAPI::returnUNKNOWN;
	} catch (const std::exception &e) {
		message = std::string("Failed to process command: check_logfile: ") + e.what();
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = "Failed to process command: check_logfile";
		return NSCAPI::returnUNKNOWN;
	}
}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(CheckLogFile)
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
