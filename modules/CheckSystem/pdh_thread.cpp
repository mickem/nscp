//////////////////////////////////////////////////////////////////////////
// PDH Collector
//
// Functions from this file collects data from the PDH subsystem and stores
// it for later use
// *NOTICE* that this is done in a separate thread so threading issues has
// to be handled. I handle threading issues in the CounterListener's get/
// set accessors.
//
// Copyright (c) 2004 MySolutions NORDIC (http://www.medin.nu)
//
// Date: 2004-03-13
// Author: Michael Medin - <michael@medin.name>
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////
#include "pdh_thread.hpp"
#include <sysinfo.h>
#include "settings.hpp"

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <parsers/filter/realtime_helper.hpp>
#include "realtime_data.hpp"

typedef parsers::where::realtime_filter_helper<check_cpu_filter::runtime_data, filters::filter_config_object> cpu_filter_helper;
typedef parsers::where::realtime_filter_helper<check_mem_filter::runtime_data, filters::filter_config_object> mem_filter_helper;

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
	CheckMemory memchecker;

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

	bool has_realtime = !filters_.empty();
	cpu_filter_helper cpu_helper(core, plugin_id);
	mem_filter_helper mem_helper(core, plugin_id);
	BOOST_FOREACH(boost::shared_ptr<filters::filter_config_object> object, filters_.get_object_list()) {
		if (object->check == "memory") {
			check_mem_filter::runtime_data data;
			BOOST_FOREACH(const std::string &d, object->data) {
				data.add(d);
			}
			mem_helper.add_item(object, data);
		} else {
			check_cpu_filter::runtime_data data;
			BOOST_FOREACH(const std::string &d, object->data) {
				data.add(d);
			}
			cpu_helper.add_item(object, data);
		}
	}

	cpu_helper.touch_all();
	mem_helper.touch_all();

	int min_threshold = 10;
	DWORD waitStatus = 0;
	int i = 0;

	if (!check_pdh)
		NSC_LOG_MESSAGE("Not checking PDH data");

	mutex_.unlock();

	do {
		std::list<std::string>	errors;
		{

			spi_container handles = fetch_spi(errors);
			windows::system_info::cpu_load load;
			try {
				load = windows::system_info::get_cpu_load();
			} catch (...) {
				errors.push_back("Failed to get cpu load");
			}
			write_metrics(handles, load, check_pdh?&pdh:NULL, errors);
		}
		try {
			if (i == 0)
				network.fetch();
		} catch (...) {
			errors.push_back("Failed to get network metrics");
		}
		if (has_realtime && i == 0) {
			cpu_helper.process_items(this);
			mem_helper.process_items(&memchecker);
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
		ret["core " + strEx::s::xtos(i++)] = l;
	return ret;
}

pdh_thread::metrics_hash pdh_thread::get_metrics() {
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex for: cput");
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

void pdh_thread::add_counter(const PDH::pdh_object &counter) {
	configs_.push_back(counter);
}

void pdh_thread::add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
	try {
		filters_.add(proxy, key, query, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
	}
}