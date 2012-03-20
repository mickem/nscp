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
NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();

#include <settings/macros.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>

#include "eventlog_wrapper.hpp"
#include "eventlog_record.hpp"

struct real_time_thread {

	struct target_information {
		std::wstring target;
		std::wstring alias;
		std::wstring syntax;
		std::wstring ok_msg;
		std::wstring perf_msg; //
		//bool perf;
		DWORD dwLang;

	};

	struct filter_container {
		std::wstring filter;
		std::wstring alias;
	};

	target_information info;
	bool enabled_;
	//std::wstring destination_;
	unsigned long long start_age_;
	unsigned long long max_age_;
	//std::wstring syntax_;
	std::list<filter_container> filters_;
	boost::shared_ptr<boost::thread> thread_;
	HANDLE stop_event_;
	std::list<std::wstring> lists_;
	std::list<std::wstring> hit_cache_;
	boost::timed_mutex cache_mutex_;

	bool cache_;
	bool debug_;

	real_time_thread() : enabled_(false), start_age_(0), max_age_(0), debug_(false), cache_(false) {
		set_start_age(_T("30m"));
		set_max_age(_T("5m"));
	}

	void add_realtime_filter(std::wstring key, std::wstring query);
	void set_enabled(bool flag) { enabled_ = flag; } 
	void set_destination(std::wstring dst) { info.target = dst; } 
	void set_start_age(std::wstring age) {
		start_age_ = strEx::stoi64_as_time(age);
	} 
	void set_max_age(std::wstring age) {
		if (age == _T("none") || age == _T("infinite") || age == _T("false"))
			max_age_ = 0;
		else
			max_age_ = strEx::stoi64_as_time(age);
	} 
	void set_eventlog(std::wstring log) {
		lists_ = strEx::splitEx(log, _T(","));
	} 

	void set_language(std::string lang);
	void set_filter(std::wstring flt) {
		add_realtime_filter(_T("filter"), flt);
	}
	bool has_filters() {
		return !filters_.empty();
	}
	bool start();
	bool stop();

	bool check_cache(unsigned long &count, std::wstring &messages);

	void thread_proc();
//	void process_events(eventlog_filter::filter_engine engine, eventlog_wrapper &eventlog);
	void process_no_events(std::wstring alias);
	void process_record(std::wstring alias, const EventLogRecord &record);
	void debug_miss(const EventLogRecord &record);
//	void process_event(eventlog_filter::filter_engine engine, const EVENTLOGRECORD* record);
};

class CheckEventLog : public nscapi::impl::simple_command_handler, public nscapi::impl::simple_plugin {
private:
	bool debug_;
	std::wstring syntax_;
	int buffer_length_;
	bool lookup_names_;
	real_time_thread thread_;

public:
	CheckEventLog();
	virtual ~CheckEventLog();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	static std::wstring getModuleName() {
		return _T("Event log Checker.");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("Check for errors and warnings in the event log.\nThis is only supported through NRPE so if you plan to use only NSClient this wont help you at all.");
	}

	void parse(std::wstring expr);

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	NSCAPI::nagiosReturn checkCache(std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf);
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn insert_eventlog(std::vector<std::wstring> arguments, std::wstring &message);

};
