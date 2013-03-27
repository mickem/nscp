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

#pragma once

#include "filter_config_object.hpp"

struct real_time_thread {
	bool enabled_;
	boost::shared_ptr<boost::thread> thread_;
	filters::filter_config_handler filters_;
	std::wstring logs_;

#ifdef WIN32
	HANDLE stop_event_;
#else
	int stop_event_[2];
#endif


	bool cache_;
	bool debug_;
	std::string filters_path_;

	real_time_thread() : enabled_(false), debug_(false), cache_(false) {}

	void add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
	void set_enabled(bool flag) { enabled_ = flag; } 

	void set_language(std::string lang);
	void set_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string flt) {
		if (!flt.empty())
			add_realtime_filter(proxy, "default", flt);
	}
	bool has_filters() {
		return !filters_.has_objects();
	}
	bool start();
	bool stop();

	void thread_proc();
	void process_object(filters::filter_config_object &object);
	void process_timeout(const filters::filter_config_object &object);
};
