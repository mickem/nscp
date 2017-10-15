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

#pragma once

#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>

#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_query.hpp>

#include <nscapi/nscapi_settings_proxy.hpp>

#include <rrd_buffer.hpp>
#include <win_sysinfo/win_sysinfo.hpp>
#include <error/error.hpp>

#include "filter_config_object.hpp"
#include "check_network.hpp"



struct spi_container {
	long long handles;
	long long procs;
	long long threads;
	spi_container() : handles(0), procs(0), threads(0) {}
};


class pdh_thread {
public:
	typedef boost::variant<std::string, long long, double> value_type;
	typedef boost::unordered_map<std::string, value_type> metrics_hash;


private:
	typedef boost::unordered_map<std::string, PDH::pdh_instance> lookup_type;
	typedef std::list<std::string> error_list;

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
	network_check::network_data network;
public:

	std::string subsystem;
	std::string disable_;
	std::string default_buffer_size;

public:

	pdh_thread(nscapi::core_wrapper *core, int plugin_id) : core(core), plugin_id(plugin_id) {
		mutex_.lock();
	}
	void add_counter(const PDH::pdh_object &counter);

	std::map<std::string, double> get_value(std::string counter);
	std::map<std::string, double> get_average(std::string counter, long seconds);
	std::map<std::string, long long> get_int_value(std::string counter);
	std::map<std::string, windows::system_info::load_entry> get_cpu_load(long seconds);

	network_check::nics_type get_network();
	metrics_hash get_metrics();

	bool start();
	bool stop();
	void set_path(const std::string mem_path, const std::string cpu_path, const std::string proc_path, const std::string legacy_path);

	void add_realtime_mem_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
	void add_realtime_cpu_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
	void add_realtime_proc_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
	void add_realtime_legacy_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);

	void add_samples(boost::shared_ptr<nscapi::settings_proxy> settings);

	std::string to_string() const { return "pdh";  }

private:
	spi_container fetch_spi(error_list &errors);
	void write_metrics(const spi_container &handles, const windows::system_info::cpu_load &load, PDH::PDHQuery *pdh, error_list &errors);

	filters::mem::filter_config_handler mem_filters_;
	filters::cpu::filter_config_handler cpu_filters_;
	filters::proc::filter_config_handler proc_filters_;
	filters::legacy::filter_config_handler legacy_filters_;

	void thread_proc();
};
