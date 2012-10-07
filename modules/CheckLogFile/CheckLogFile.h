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
NSC_WRAPPERS_MAIN()

#include <settings/macros.h>
#include <strEx.h>
#include <utils.h>
#include <checkHelpers.hpp>

#include "filters.hpp"


struct real_time_thread {
	bool enabled_;
	boost::shared_ptr<boost::thread> thread_;
	filters::filter_config_handler filters_;
	std::wstring logs_;

#ifdef WIN32
	HANDLE stop_event_;
#endif


	bool cache_;
	bool debug_;
	std::wstring filters_path_;

	real_time_thread() : enabled_(false), debug_(false), cache_(false) {}

	void add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring key, std::wstring query);
	void set_enabled(bool flag) { enabled_ = flag; } 

	void set_language(std::string lang);
	void set_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring flt) {
		if (!flt.empty())
			add_realtime_filter(proxy, _T("default"), flt);
	}
	bool has_filters() {
		return !filters_.has_objects();
	}
	bool start();
	bool stop();

	void thread_proc();
	void process_object(filters::filter_config_object &object);
	void process_timeout(const filters::filter_config_object &object);
	//void process_record(const filters::filter_config_object &object, const std::string line);
	//void debug_miss(const EventLogRecord &record);
};

class CheckLogFile : public nscapi::impl::utf8_command_handler, public nscapi::impl::simple_plugin {
private:
	bool debug_;
	std::wstring syntax_;
	int buffer_length_;
	bool lookup_names_;
	real_time_thread thread_;

public:
	CheckLogFile();
	virtual ~CheckLogFile();
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
	NSCAPI::nagiosReturn handleCommand(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &message, std::string &perf);

};
