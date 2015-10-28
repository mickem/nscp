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

#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>
#include <boost/variant.hpp>

#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_query.hpp>

#include <win_sysinfo/win_sysinfo.hpp>
#include "filter_config_object.hpp"

#include <nscapi/nscapi_settings_proxy.hpp>

#include <error.hpp>

/**
 * @ingroup NSClientCompat
 * PDH collector thread (gathers performance data and allows for clients to retrieve it)
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 *
 */
template<class T>
struct rrd_buffer {
	typedef T value_type;
	typedef boost::circular_buffer<T> list_type;
	list_type seconds;
	list_type minutes;
	list_type hours;
	int second_counter;
	int minute_counter;

public:
	rrd_buffer() : second_counter(0), minute_counter(0) {
		seconds.resize(60);
		minutes.resize(60);
		hours.resize(24);
	}
	value_type get_average(long time) const {
		value_type ret;
		if (time < 0)
			return ret;
		if (time <= seconds.size()) {
			for (list_type::const_iterator cit = seconds.end() - time; cit != seconds.end(); ++cit) {
				ret.add(*cit);
			}
			ret.normalize(time);
			return ret;
		}
		time /= 60;
		if (time <= minutes.size()) {
			for (list_type::const_iterator cit = minutes.end() - time; cit != minutes.end(); ++cit) {
				ret.add(*cit);
			}
			ret.normalize(time);
			return ret;
		}
		time /= 60;
		if (time >= hours.size())
			throw nscp_exception("Size larger than buffer");
		for (list_type::const_iterator cit = hours.end() - time; cit != hours.end(); ++cit) {
			ret.add(*cit);
		}
		ret.normalize(time);
		return ret;
	}
	value_type calculate_avg(list_type &buffer) const {
		value_type ret;
		BOOST_FOREACH(const value_type &entry, buffer) {
			ret.add(entry);
		}
		ret.normalize(buffer.size());
		return ret;
	}

	void push(const value_type &value) {
		seconds.push_back(value);
		if (second_counter++ >= 59) {
			second_counter = 0;
			windows::system_info::cpu_load avg = calculate_avg(seconds);
			minutes.push_back(avg);
			if (minute_counter++ >= 59) {
				minute_counter = 0;
				windows::system_info::cpu_load avg = calculate_avg(minutes);
				hours.push_back(avg);
			}
		}
	}
};

class pdh_thread {
public:
	typedef boost::variant<std::string, long long, double> value_type;
	typedef boost::unordered_map<std::string, value_type> metrics_hash;

private:
	typedef boost::unordered_map<std::string, PDH::pdh_instance> lookup_type;

	boost::shared_ptr<boost::thread> thread_;
	boost::shared_mutex mutex_;
	HANDLE stop_event_;
	int plugin_id;
	nscapi::core_wrapper *core;

	metrics_hash metrics;

	std::list<PDH::pdh_object> configs_;
	std::list<PDH::pdh_instance> counters_;
	rrd_buffer<windows::system_info::cpu_load> cpu;
	lookup_type lookups_;
public:

	std::string subsystem;
	std::string default_buffer_size;
	std::string filters_path_;

public:

	pdh_thread(nscapi::core_wrapper *core, int plugin_id) : core(core), plugin_id(plugin_id) {
		mutex_.lock();
	}
	void add_counter(const PDH::pdh_object &counter);

	std::map<std::string, double> get_value(std::string counter);
	std::map<std::string, double> get_average(std::string counter, long seconds);
	std::map<std::string, long long> get_int_value(std::string counter);
	std::map<std::string, windows::system_info::load_entry> get_cpu_load(long seconds);
	metrics_hash get_metrics();

	bool start();
	bool stop();

	void add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);

private:
	filters::filter_config_handler filters_;

	void thread_proc();
};