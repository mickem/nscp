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

#include <map>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/cmdline.hpp>

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
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, NSCAPI::returnOK, object.ok_msg, object.perf_msg, response)) {
		NSC_LOG_ERROR(_T("Failed to submit evenhtlog result: ") + response);
	}
}

void real_time_thread::process_object(const filters::filter_config_object &object) {
	std::wstring response;
	int severity = object.severity;
	std::wstring command = object.alias;
	if (severity == -1) {
		NSC_LOG_ERROR(_T("Severity not defined for: ") + object.alias);
		severity = NSCAPI::returnUNKNOWN;
	}
	if (!object.command.empty())
		command = object.command;
// 	std::wstring message = record.render(true, object.syntax, object.date_format, object.dwLang);
// 	if (!nscapi::core_helper::submit_simple_message(object.target, command, object.severity, message, object.perf_msg, response)) {
// 		NSC_LOG_ERROR(_T("Failed to submit eventlog result ") + object.alias + _T(": ") + response);
// 	}
}

// void real_time_thread::debug_miss(const EventLogRecord &record) {
// // 	std::wstring message = record.render(true, _T("%id% %level% %source%: %message%"), DATE_FORMAT, LANG_NEUTRAL);
// // 	NSC_DEBUG_MSG_STD(_T("No filter matched: ") + message);
// }

void real_time_thread::thread_proc() {

	std::list<filters::filter_config_object> filters;
	std::list<std::wstring> logs;

	BOOST_FOREACH(std::wstring s, strEx::splitEx(logs_, _T(","))) {
		logs.push_back(s);
	}

	BOOST_FOREACH(filters::filter_config_object object, filters_.get_object_list()) {
		logfile_filter::filter_argument fargs = logfile_filter::factories::create_argument(object.syntax, object.date_format);
		fargs->debug = object.debug;
		fargs->alias = object.alias;
		fargs->bShowDescriptions = true;
		if (object.log_ != _T("any") && object.log_ != _T("all"))
			logs.push_back(object.log_);
		// eventlog_filter::filter_engine 
		object.engine = logfile_filter::factories::create_engine(fargs, utf8::cvt<std::string>(object.filter));

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
	}
	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD(_T("Scanning logs: ") + strEx::joinEx(logs, _T(", ")));

// 	typedef boost::shared_ptr<eventlog_wrapper> eventlog_type;
// 	typedef std::vector<eventlog_type> eventlog_list;
// 	eventlog_list evlog_list;
	
	// TODO: add support for scanning "missed messages" at startup

// 	HANDLE *handles = new HANDLE[1+evlog_list.size()];
// 	handles[0] = stop_event_;
// 	for (int i=0;i<evlog_list.size();i++) {
// 		evlog_list[i]->notify(handles[i+1]);
// 	}
// 	BOOST_FOREACH(filters::filter_config_object &object, filters) {
// 		object.touch(ltime);
// 	}

// 	unsigned int errors = 0;
// 	while (true) {
// 
// 		DWORD minNext = INFINITE;
// 		BOOST_FOREACH(const filters::filter_config_object &object, filters) {
// 			NSC_DEBUG_MSG_STD(_T("Getting next from: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
// 			if (object.next_ok_ > 0 && object.next_ok_ < minNext)
// 				minNext = object.next_ok_;
// 		}

// 		if (ltime > minNext) {
// 			NSC_LOG_ERROR(_T("Strange seems we expect to send ok now?"));
// 			continue;
// 		}
// 		DWORD dwWaitTime = (minNext - ltime)*1000;
// 		if (minNext == INFINITE || dwWaitTime < 0)
// 			dwWaitTime = INFINITE;
// 		NSC_DEBUG_MSG(_T("Next miss time is in: ") + strEx::itos(dwWaitTime) + _T("s"));

// 		std::string changed_folder = impl->wait_for_change();
// 		BOOST_FOREACH(filters::filter_config_object &object, filters) {
// 			if (in_folder(object, changed_folder)) {
// 				object.touch(ltime);
// 				process_object(object);
// 			}
// 		}

// 		_time64(&ltime);
// 		BOOST_FOREACH(filters::filter_config_object &object, filters) {
// 			if (object.next_ok_ != 0 && object.next_ok_ <= (ltime+1)) {
// 				process_timeout(object);
// 				object.touch(ltime);
// 			} else {
// 				NSC_DEBUG_MSG_STD(_T("missing: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
// 			}
// 		}
// 	}
// 	delete [] handles;
	return;
}


bool real_time_thread::start() {
	if (!enabled_)
		return true;

// 	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("EventLogShutdown"));

	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&real_time_thread::thread_proc, this)));
	return true;
}
bool real_time_thread::stop() {
// 	SetEvent(stop_event_);
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
		settings.set_alias(alias, _T("eventlog"));
		
		thread_.filters_path_ = settings.alias().get_settings_path(_T("real-time/filters"));


		settings.alias().add_path_to_settings()
			(_T("LOG FILE SECTION"), _T("Section for log file checker"))

			(_T("real-time"), _T("CONFIGURE REALTIME CHECKING"), _T("A set of options to configure the real time checks"))

			(_T("real-time/filters"), sh::fun_values_path(boost::bind(&real_time_thread::add_realtime_filter, &thread_, get_settings_proxy(), _1, _2)),  
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

template<class Tobject, class THandler>
struct filter_text_renderer {
	typedef boost::function<std::wstring(Tobject*)> function_type;
	struct my_entry {
		parsers::simple_expression::entry origin;
		function_type fun;
		my_entry(const parsers::simple_expression::entry &origin) : origin(origin) {}
		my_entry(const my_entry &other) : origin(other.origin), fun(other.fun) {}
		const my_entry& operator= (const my_entry &other) {
			origin = other.origin;
			fun = other.fun;
			return *this;
		}
	};

	typedef std::vector<my_entry> entry_list;
	entry_list entries;

	bool parse(const std::string str, std::string &error) {
		parsers::simple_expression::result_type keys;
		parsers::simple_expression expr;
		if (!expr.parse(str, keys)) {
			error = "Failed to parse: " + str;
			return false;
		}
		THandler handler;
		BOOST_FOREACH(const parsers::simple_expression::entry &e, keys) {
			my_entry my_e(e);
			if (e.is_variable) {
				std::wstring tag = utf8::cvt<std::wstring>(e.name);
				if (!handler.has_variable(tag)) {
					error = "Invalid variable: " + e.name;
					return false;
				}
				my_e.fun = handler.bind_simple_string(tag);
				if (!my_e.fun) {
					error = "Invalid variable: " + e.name;
					return false;
				}
			}
			entries.push_back(my_e);
		}
		return true;
	}
	std::string render(boost::shared_ptr<Tobject> obj) {
		std::string ret;
		BOOST_FOREACH(const my_entry &e, entries) {
			if (!e.origin.is_variable)
				ret += e.origin.name;
			else {
				ret += utf8::cvt<std::string>(e.fun(obj.get()));
			}
		}
		return ret;
	}
};

template<class Tsummary>
struct text_renderer {
	typedef boost::function<std::string(Tsummary*)> function_type;
	struct my_entry {
		parsers::simple_expression::entry origin;
		function_type fun;
		my_entry(const parsers::simple_expression::entry &origin) : origin(origin) {}
		my_entry(const my_entry &other) : origin(other.origin), fun(other.fun) {}
		const my_entry& operator= (const my_entry &other) {
			origin = other.origin;
			fun = other.fun;
			return *this;
		}
	};

	typedef std::vector<my_entry> entry_list;
	entry_list entries;

	bool parse(const std::string str, std::string &error) {
		parsers::simple_expression::result_type keys;
		parsers::simple_expression expr;
		if (!expr.parse(str, keys)) {
			error = "Failed to parse: " + str;
			return false;
		}
		BOOST_FOREACH(const parsers::simple_expression::entry &e, keys) {
			my_entry my_e(e);
			if (e.is_variable) {
				my_e.fun = Tsummary::get_function(e.name);
				if (!my_e.fun) {
					error = "Invalid variable: " + e.name;
					return false;
				}
			}
			entries.push_back(my_e);
		}
		return true;
	}
	std::string render(Tsummary &data) {
		std::string ret;
		BOOST_FOREACH(const my_entry &e, entries) {
			if (!e.origin.is_variable)
				ret += e.origin.name;
			else {
				ret += e.fun(&data);
			}
		}
		return ret;
	}
};


struct log_summary {
	long long count;
	std::string message;
	std::string error;

	log_summary() : count(0) {}

	std::string get_message() {
		return message;
	}
	std::string get_error() {
		return error;
	}
	std::string get_count() {
		return strEx::s::xtos(count);
	}
	static boost::function<std::string(log_summary*)> get_function(std::string key) {
		if (key == "messages" || key == "lines")
			return &log_summary::get_message;
		if (key == "error" || key == "last")
			return &log_summary::get_error;
		if (key == "count")
			return &log_summary::get_count;
		return boost::function<std::string(log_summary*)>();
	}
};

typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsULongInteger> EventLogQuery1Container;
typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULongInteger> EventLogQuery2Container;
NSCAPI::nagiosReturn CheckLogFile::handleCommand(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &message, std::string &perf) {
	if (command != "check_logfile" && command != "checklogfile")
		return NSCAPI::returnIgnored;
	simple_timer time;
	
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> files;
	logfile_filter::filter_argument fargs = logfile_filter::factories::create_argument(syntax_, DATE_FORMAT);

	std::string regexp;
	std::string column_split;
	std::string line_split;
	std::string filter_string, warn_string, crit_string, ok_string;
	std::string syntax_top, syntax_detail;
	std::vector<std::string> file_list;
	std::string files_string;
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

		text_renderer<log_summary> renderer_top;
		if (!renderer_top.parse(syntax_top, message)) {
			return NSCAPI::returnUNKNOWN;
		}

		filter_text_renderer<logfile_filter::filter_obj, logfile_filter::filter_obj_handler> renderer_detail;
		if (!renderer_detail.parse(syntax_detail, message)) {
			return NSCAPI::returnUNKNOWN;
		}

		logfile_filter::filter_engine filter = logfile_filter::factories::create_engine(fargs, filter_string);
		logfile_filter::filter_engine warn, crit, ok;
		if (!warn_string.empty()) warn = logfile_filter::factories::create_engine(fargs, warn_string);
		if (!crit_string.empty()) crit = logfile_filter::factories::create_engine(fargs, crit_string);
		if (!ok_string.empty()) ok = logfile_filter::factories::create_engine(fargs, ok_string);

		if (!filter) {
			message = "Failed to initialize filter subsystem.";
			return NSCAPI::returnUNKNOWN;
		}

		filter->boot();
		if (warn) warn->boot();
		if (crit) crit->boot();
		if (ok) ok->boot();

		if (!column_split.empty()) {
			strEx::replace(column_split, "\\t", "\t");
			strEx::replace(column_split, "\\n", "\n");
		}

		std::wstring msg;
		if (!filter->validate(msg)) {
			message = utf8::cvt<std::string>(msg);
			return NSCAPI::returnUNKNOWN;
		}
		if (warn && !warn->validate(msg)) {
			message = utf8::cvt<std::string>(msg);
			return NSCAPI::returnUNKNOWN;
		}
		if (crit && !crit->validate(msg)) {
			message = utf8::cvt<std::string>(msg);
			return NSCAPI::returnUNKNOWN;
		}
		if (ok && !ok->validate(msg)) {
			message = utf8::cvt<std::string>(msg);
			return NSCAPI::returnUNKNOWN;
		}

		NSC_DEBUG_MSG_STD(_T("Boot time: ") + strEx::itos(time.stop()));

		log_summary summary;
		BOOST_FOREACH(const std::string &filename, file_list) {
			std::ifstream file(filename.c_str());
			if (file.is_open()) {
				std::string line;
				while (file.good()) {
					std::getline(file,line, '\n');
					if (!column_split.empty()) {
						std::list<std::string> chunks = strEx::s::splitEx(line, column_split);
						boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(filename, line, chunks, summary.count));
						if (filter->match(record)) {
							record->matched();
							std::string current = renderer_detail.render(record);
							summary.count++;
							if (crit && crit->match(record)) {
								summary.error = current;
								message = renderer_top.render(summary);
								return NSCAPI::returnCRIT;
							} else if (warn && warn->match(record)) {
								summary.error = current;
								message = renderer_top.render(summary);
								return NSCAPI::returnWARN;
							} else if (ok && ok->match(record)) {
								summary.error = current;
								message = renderer_top.render(summary);
								return NSCAPI::returnOK;
							} else if (debug) {
								NSC_DEBUG_MSG_STD(_T("Crit/Warn/* did not match: ") + utf8::cvt<std::wstring>(current));
							}
							summary.message += current;
						}
					}
				}
				file.close();
			}
		}
		NSC_DEBUG_MSG_STD(_T("Evaluation time: ") + strEx::itos(time.stop()));

		if (message.empty())
			message = "OK, no matches found";
		return returnCode;
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
