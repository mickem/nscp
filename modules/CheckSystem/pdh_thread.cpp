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

#include "pdh_thread.hpp"
#include <sysinfo.h>
#include "settings.hpp"

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include "realtime_data.hpp"
#include "check_memory.hpp"
#include "check_process.hpp"

typedef parsers::where::realtime_filter_helper<check_cpu_filter::runtime_data, filters::cpu::filter_config_object> cpu_filter_helper;

struct NSC_error_pdh : public process_helper::error_reporter {
	std::list<std::string> l;
	void report_error(std::string error) {
		//l.push_back(error);
	}
	void report_warning(std::string error) {}
	void report_debug(std::string error) {}
};

spi_container pdh_thread::fetch_spi(error_list &errors) {
	spi_container ret;
	try {
		hlp::buffer<BYTE, windows::winapi::SYSTEM_PROCESS_INFORMATION*> buffer = windows::system_info::get_system_process_information();
		windows::winapi::SYSTEM_PROCESS_INFORMATION* b = buffer.get();
		while (b != NULL) {
			ret.handles += b->HandleCount;
			ret.threads += b->NumberOfThreads;
			ret.procs++;

			if (b->NextEntryOffset == NULL)
				return ret;
			b = (windows::winapi::SYSTEM_PROCESS_INFORMATION*)((PCHAR)b + b->NextEntryOffset);
		}
	} catch (...) {
		errors.push_back("Failed to get metrics");
	}
	return ret;
}

bool first = true;

void pdh_thread::write_metrics(const spi_container &handles, const windows::system_info::cpu_load &load, PDH::PDHQuery *pdh, error_list &errors) {
	boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!writeLock.owns_lock()) {
		errors.push_back("Failed to get mutex for writing");
		return;
	}
	try {
		cpu.push(load);
		if (pdh != NULL)
			pdh->gatherData();

		BOOST_FOREACH(const lookup_type::value_type &e, lookups_) {
			if (e.second->has_instances()) {
				BOOST_FOREACH(const PDH::pdh_instance i, e.second->get_instances()) {
					metrics["pdh." + e.first + "." + i->get_name()] = i->get_int_value();
				}
			} else {
				metrics["pdh." + e.first] = e.second->get_int_value();
			}
		}

		metrics["procs.handles"] = handles.handles;
		metrics["procs.threads"] = handles.threads;
		metrics["procs.procs"] = handles.procs;
	} catch (const PDH::pdh_exception &e) {
		if (first) {
			// If this is the first run an error will be thrown since the data is not yet available
			// This is "ok" but perhaps another solution would be better, but this works :)
			first = false;
		} else {
			errors.push_back("Failed to query performance counters: " + e.reason());
		}
	} catch (...) {
		errors.push_back("Failed to query performance counters: ");
	}
}


/**
* Thread that collects the data every "CHECK_INTERVAL" seconds.
*
* @param lpParameter Not used
* @return thread exit status
*
* @author mickem
*
* @date 03-13-2004
*
* @bug If we have "custom named" counters ?
* @bug This whole concept needs work I think.
*
*/
void pdh_thread::thread_proc() {

	memory_checks::realtime::helper memory_helper(core, plugin_id);
	process_checks::realtime::helper process_helper(core, plugin_id);

	if (subsystem == "fast" || subsystem == "auto" || subsystem == "default") {
		PDH::factory::set_native();
	} else if (subsystem == "thread-safe") {
		PDH::factory::set_thread_safe();
	} else {
		NSC_LOG_ERROR_STD("Unknown PDH subsystem valid values are: fast (default) and thread-safe");
	}
	{
		PDH::PDHQuery tmpPdh;
		BOOST_FOREACH(PDH::pdh_object obj, configs_) {
			try {
				PDH::pdh_instance instance = PDH::factory::create(obj);

				if (instance->has_instances()) {
					BOOST_FOREACH(PDH::pdh_instance sc, instance->get_instances()) {
						tmpPdh.addCounter(sc);
					}
				} else {
					tmpPdh.addCounter(instance);
				}

				tmpPdh.open();
				counters_.push_back(instance);
				lookups_[instance->get_name()] = instance;
				tmpPdh.close();
			} catch (const std::exception &e) {
				NSC_LOG_ERROR_EX("Failed to add counter " + obj.alias + ": ", e);
				continue;
			}
		}
	}

	PDH::PDHQuery pdh;

	bool check_pdh = !counters_.empty();

	if (check_pdh) {
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT));
		// 		boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
		// 		if (!writeLock.owns_lock()) {
		// 			NSC_LOG_ERROR_STD("Failed to get mutex when trying to start thread.");
		// 			return;
		// 		}
		pdh.removeAllCounters();
		BOOST_FOREACH(PDH::pdh_instance c, counters_) {
			try {
				if (c->has_instances()) {
					BOOST_FOREACH(PDH::pdh_instance sc, c->get_instances()) {
						NSC_DEBUG_MSG("Loading counter: " + sc->get_name() + " = " + sc->get_counter());
						pdh.addCounter(sc);
					}
				} else {
					NSC_DEBUG_MSG("Loading counter: " + c->get_name() + " = " + c->get_counter());
					pdh.addCounter(c);
				}
			} catch (...) {
				NSC_LOG_ERROR_WA("EXCEPTION: Failed to load counter: " + c->get_name() + " = ", c->get_counter());
			}
		}
		try {
			pdh.open();
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("Failed to open counters (performance counters disabled)", e);
			check_pdh = false;
		}
		try {
			pdh.collect();
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("Failed to collect counters (performance counters disabled): ", e);
			check_pdh = false;
		}
	}

	bool has_mem_realtime = !mem_filters_.empty() || !legacy_filters_.empty();
	bool has_cpu_realtime = !cpu_filters_.empty() || !legacy_filters_.empty();
	bool has_proc_realtime = !proc_filters_.empty();
	bool has_realtime = has_mem_realtime || has_cpu_realtime || has_proc_realtime;
	if (!legacy_filters_.empty()) {
		NSC_LOG_MESSAGE("You are using legacy filters in check system, please migrate to new filters...");
	}
	cpu_filter_helper cpu_helper(core, plugin_id);
	BOOST_FOREACH(boost::shared_ptr<filters::legacy::filter_config_object> object, legacy_filters_.get_object_list()) {
		if (object->check == "memory") {
			memory_helper.add_obj(boost::shared_ptr<filters::mem::filter_config_object>(new filters::mem::filter_config_object(*object)));
		} else {
			check_cpu_filter::runtime_data data;
			BOOST_FOREACH(const std::string &d, object->data) {
				data.add(d);
			}
			cpu_helper.add_item(boost::shared_ptr<filters::cpu::filter_config_object>(new filters::cpu::filter_config_object(*object)), data, "system.cpu");
		}
	}
	BOOST_FOREACH(boost::shared_ptr<filters::mem::filter_config_object> object, mem_filters_.get_object_list()) {
		memory_helper.add_obj(object);
	}
	BOOST_FOREACH(boost::shared_ptr<filters::cpu::filter_config_object> object, cpu_filters_.get_object_list()) {
		check_cpu_filter::runtime_data data;
		BOOST_FOREACH(const std::string &d, object->data) {
			data.add(d);
		}
		cpu_helper.add_item(object, data, "system.cpu");
	}
	BOOST_FOREACH(boost::shared_ptr<filters::proc::filter_config_object> object, proc_filters_.get_object_list()) {
		process_helper.add_obj(object);
	}

	cpu_helper.touch_all();
	memory_helper.boot();
	process_helper.boot();

	int min_threshold = 10;
	DWORD waitStatus = 0;
	int i = 0;

	if (!check_pdh)
		NSC_LOG_MESSAGE("Not checking PDH data");
	if (has_realtime)
		NSC_DEBUG_MSG("Real time checks enabled");

	mutex_.unlock();

	bool disable_network = disable_.find("network") != std::string::npos;
	if (disable_network) {
		NSC_LOG_MESSAGE("WARNING: network checking is disabled");
	}
	bool disable_handles = disable_.find("handles") != std::string::npos;
	if (disable_handles) {
		NSC_LOG_MESSAGE("WARNING: handle checking is disabled");
	}
	bool disable_cpu = disable_.find("cpu") != std::string::npos;
	if (disable_cpu) {
		NSC_LOG_MESSAGE("WARNING: cpu checking is disabled");
	}
	bool disable_metrics = disable_.find("metrics") != std::string::npos;
	if (disable_metrics) {
		NSC_LOG_MESSAGE("WARNING: metrics writing is disabled");
	}
	bool disable_pdh = disable_.find("pdh") != std::string::npos;
	if (disable_pdh) {
		check_pdh = false;
		NSC_LOG_MESSAGE("WARNING: pdh writing is disabled");
	}
	spi_container handles;
	do {
		std::list<std::string>	errors;
		{
			if (!disable_handles && i == 0) {
				try {
					handles = fetch_spi(errors);
				} catch (...) {
					errors.push_back("Failed to get handles");
				}
			}
			windows::system_info::cpu_load load;
			if (!disable_cpu) {
				try {
					load = windows::system_info::get_cpu_load();
				} catch (...) {
					errors.push_back("Failed to get cpu load");
				}
			}
			if (!disable_metrics) {
				write_metrics(handles, load, check_pdh ? &pdh : NULL, errors);
			}
		}
		try {
			if (i == 0 && !disable_network)
				network.fetch();
		} catch (const nsclient::nsclient_exception &e) {
			errors.push_back("Failed to get network metrics: " + e.reason());
		} catch (const std::exception &e) {
			errors.push_back("Failed to get network metrics: " + utf8::utf8_from_native(e.what()));
		} catch (...) {
			errors.push_back("Failed to get network metrics");
		}
		if (has_realtime && i == (min_threshold-1)) {
			if (has_cpu_realtime)
				cpu_helper.process_items(this);
			if (has_mem_realtime)
				memory_helper.check();
			if (has_proc_realtime)
				process_helper.check();
		}
		if (i++ > min_threshold)
			i = 0;
		BOOST_FOREACH(const std::string &s, errors) {
			NSC_LOG_ERROR(s);
		}
	} while (((waitStatus = WaitForSingleObject(stop_event_, 1000)) == WAIT_TIMEOUT));
	if (waitStatus != WAIT_OBJECT_0) {
		NSC_LOG_ERROR("Something odd happened when terminating PDH collection thread!");
		return;
	}

	if (check_pdh) {
		boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!writeLock.owns_lock()) {
			NSC_LOG_ERROR("Failed to get Mute when closing thread!");
		}
		try {
			pdh.close();
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("Failed to close performance counters: ", e);
		}
	}
}

void pdh_thread::add_samples(boost::shared_ptr<nscapi::settings_proxy> settings) {
	mem_filters_.add_samples(settings);
	cpu_filters_.add_samples(settings);
	proc_filters_.add_samples(settings);
	legacy_filters_.add_samples(settings);
}

std::map<std::string, long long> pdh_thread::get_int_value(std::string counter) {
	std::map<std::string, long long> ret;
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex for: " + counter);
		return ret;
	}

	lookup_type::iterator it = lookups_.find(counter);
	if (it == lookups_.end()) {
		NSC_LOG_ERROR("Counter was not found: " + counter);
		return ret;
	}
	const PDH::pdh_instance ptr = (*it).second;
	if (ptr->has_instances()) {
		BOOST_FOREACH(const PDH::pdh_instance i, ptr->get_instances()) {
			ret[i->get_name()] = i->get_int_value();
		}
	} else {
		ret[ptr->get_name()] = ptr->get_int_value();
	}
	return ret;
}

std::map<std::string, double> pdh_thread::get_value(std::string counter) {
	std::map<std::string, double> ret;
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex for: " + counter);
		return ret;
	}

	lookup_type::iterator it = lookups_.find(counter);
	if (it == lookups_.end()) {
		NSC_LOG_ERROR("Counter was not found: " + counter);
		return ret;
	}
	const PDH::pdh_instance ptr = (*it).second;
	if (ptr->has_instances()) {
		BOOST_FOREACH(const PDH::pdh_instance i, ptr->get_instances()) {
			ret[i->get_name()] = i->get_value();
		}
	} else {
		ret[ptr->get_name()] = ptr->get_value();
	}
	return ret;
}

std::map<std::string, double> pdh_thread::get_average(std::string counter, long seconds) {
	std::map<std::string, double> ret;
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex for: " + counter);
		return ret;
	}

	lookup_type::iterator it = lookups_.find(counter);
	if (it == lookups_.end()) {
		NSC_LOG_ERROR("Counter was not found: " + counter);
		return ret;
	}
	const PDH::pdh_instance ptr = (*it).second;
	if (ptr->has_instances()) {
		BOOST_FOREACH(const PDH::pdh_instance i, ptr->get_instances()) {
			ret[i->get_name()] = i->get_average(seconds);
		}
	} else {
		ret[ptr->get_name()] = ptr->get_average(seconds);
	}
	return ret;
}

network_check::nics_type pdh_thread::get_network() {
	return network.get();
}

std::map<std::string, windows::system_info::load_entry> pdh_thread::get_cpu_load(long seconds) {
	std::map<std::string, windows::system_info::load_entry> ret;
	windows::system_info::cpu_load load;
	{
		boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			NSC_LOG_ERROR("Failed to get Mutex for: cpu");
			return ret;
		}
		load = cpu.get_average(seconds);
	}
	ret["total"] = load.total;
	int i = 0;
	BOOST_FOREACH(const windows::system_info::load_entry &l, load.core)
		ret["core " + str::xtos(i++)] = l;
	return ret;
}

pdh_thread::metrics_hash pdh_thread::get_metrics() {
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex for: metrics");
		return metrics_hash();
	}
	return metrics_hash(metrics);
}

bool pdh_thread::start() {
	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("EventLogShutdown"));
	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&pdh_thread::thread_proc, this)));
	return true;
}
bool pdh_thread::stop() {
	SetEvent(stop_event_);
	if (thread_)
		thread_->join();
	return true;
}

void pdh_thread::set_path(const std::string mem_path, const std::string cpu_path, const std::string proc_path, const std::string legacy_path) {
	mem_filters_.set_path(mem_path);
	cpu_filters_.set_path(cpu_path);
	proc_filters_.set_path(proc_path);
	legacy_filters_.set_path(legacy_path);
}

void pdh_thread::add_counter(const PDH::pdh_object &counter) {
	configs_.push_back(counter);
}

void pdh_thread::add_realtime_mem_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
	try {
		mem_filters_.add(proxy, key, query);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
	}
}
void pdh_thread::add_realtime_cpu_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
	try {
		cpu_filters_.add(proxy, key, query);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
	}
}
void pdh_thread::add_realtime_proc_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
	try {
		proc_filters_.add(proxy, key, query);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
	}
}
void pdh_thread::add_realtime_legacy_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
	try {
		legacy_filters_.add(proxy, key, query);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
	}
}
