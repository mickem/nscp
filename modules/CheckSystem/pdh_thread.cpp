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

#include "stdafx.h"
#include "pdh_thread.hpp"
#include <sysinfo.h>
#include "settings.hpp"

#include <nscapi/nscapi_plugin_interface.hpp>

pdh_thread::pdh_thread() {
}

pdh_thread::~pdh_thread() {

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

	BOOST_FOREACH(PDH::pdh_object obj, configs_) {
		PDH::pdh_instance instance = PDH::factory::create(obj);
		counters_.push_back(instance);
		lookups_[instance->get_name()] = instance;
	}

	PDH::PDHQuery pdh;

	bool check_pdh = !counters_.empty();

	if (check_pdh) {
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT));
		boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			NSC_LOG_ERROR_STD("Failed to get mutex when trying to start thread.");
			return;
		}
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
//				c.set_default_buffer_size(default_buffer_length);
//				collector_ptr collector = c.create(check_intervall_);
// 				if (collector) {
// 					counters_[c.alias] = collector;
// 					pdh.addCounter(c.path, collector);
// 				} else {
// 					NSC_LOG_ERROR_WA("Failed to load counter: " + c.alias + " = ",  c.path);
// 				}
			} catch (...) {
				NSC_LOG_ERROR_WA("EXCEPTION: Failed to load counter: " + c->get_name() + " = ", c->get_counter());
			}
		}
		try {
			pdh.open();
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("Opening performance counters: ", e);
			return;
		}
	}



	DWORD waitStatus = 0;
	bool first = true;
	do {
		std::list<std::string>	errors;
		{
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
			if (!writeLock.owns_lock()) {
				NSC_LOG_ERROR("Failed to get Mutex!");
			} else {
				try {
					cpu.push(windows::system_info::get_cpu_load());
					if (check_pdh)
						pdh.gatherData();
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
		}
		BOOST_FOREACH(const std::string &s, errors) {
			NSC_LOG_ERROR(s);
		}
	} while (((waitStatus = WaitForSingleObject(stop_event_, 1000)) == WAIT_TIMEOUT));
	if (waitStatus != WAIT_OBJECT_0) {
		NSC_LOG_ERROR("Something odd happened when terminating PDH collection thread!");
		return;
	}

	if (check_pdh) 	{
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

std::map<std::string,long long> pdh_thread::get_int_value(std::string counter) {
	std::map<std::string,long long> ret;
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

std::map<std::string,double> pdh_thread::get_value(std::string counter) {
	std::map<std::string,double> ret;
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

std::map<std::string,double> pdh_thread::get_average(std::string counter, long seconds) {
	std::map<std::string,double> ret;
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

std::map<std::string,windows::system_info::load_entry> pdh_thread::get_cpu_load(long seconds) {
	std::map<std::string,windows::system_info::load_entry> ret;
	windows::system_info::cpu_load load;
	{
		boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			NSC_LOG_ERROR("Failed to get Mutex for: cput");
			return ret;
		}
		load = cpu.get_average(seconds);
	}
	ret["total"] = load.total;
	int i=0;
	BOOST_FOREACH(const windows::system_info::load_entry &l, load.core)
		ret["core " + strEx::s::xtos(i++)] = l;
	return ret;
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

