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
#include "PDHCollector.h"
#include <sysinfo.h>
#include "settings.hpp"

PDHCollector::PDHCollector() : stop_event_(NULL) {
}

PDHCollector::~PDHCollector()
{
	if (stop_event_ != NULL)
		CloseHandle(stop_event_);
}

boost::shared_ptr<PDHCollectors::PDHCollector> PDHCollector::system_counter_data::counter::create(int check_intervall) {
	if (data_type == type_uint64 && data_format == format_large && collection_strategy == value) {
		return boost::shared_ptr<PDHCollectors::PDHCollector>(new PDHCollectors::StaticPDHCounterListener<unsigned __int64, PDHCollectors::format_large>);
	} else if (data_type == type_int64 && data_format == format_large && collection_strategy == value) {
		return boost::shared_ptr<PDHCollectors::PDHCollector>(new PDHCollectors::StaticPDHCounterListener<__int64, PDHCollectors::format_large>);
	} else if (data_type == type_int64 && data_format == format_large && collection_strategy == rrd) {
		unsigned int buffer_size = get_buffer_length(check_intervall);
		return boost::shared_ptr<PDHCollectors::PDHCollector>(new PDHCollectors::RoundINTPDHBufferListener<__int64, PDHCollectors::format_large>(buffer_size));
	}
	return boost::shared_ptr<PDHCollectors::PDHCollector>();
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
void PDHCollector::thread_proc() {

	if (!thread_data_) {
		NSC_LOG_ERROR_STD("No configuration for PDH thread: Exiting");
		return;

	}
	if (thread_data_->subsystem == _T("fast") || thread_data_->subsystem == _T("auto") || thread_data_->subsystem == _T("default")) {
	} else if (thread_data_->subsystem == _T("thread-safe")) {
		PDH::PDHFactory::set_threadSafe();
	} else {
		NSC_LOG_ERROR_STD("Unknown PDH subsystem valid values are: fast (default) and thread-safe");
	}

	check_intervall_ = thread_data_->check_intervall;
	std::string default_buffer_length = thread_data_->buffer_length;
	PDH::PDHQuery pdh;

	if (thread_data_->counters.empty()) {
		NSC_LOG_ERROR_STD("No counters configure in PDH thread.");
		return;
	}

	{
		SetThreadLocale(MAKELCID(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),SORT_DEFAULT));
		boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
		if (!writeLock.owns_lock()) {
			NSC_LOG_ERROR_STD("Failed to get mutex when trying to start thread.");
			return;
		}
		pdh.removeAllCounters();
		BOOST_FOREACH(system_counter_data::counter c, thread_data_->counters) {
			try {
				NSC_LOG_ERROR_WA("Loading counter: " + c.alias + " = ", c.path);
				c.set_default_buffer_size(default_buffer_length);
				collector_ptr collector = c.create(check_intervall_);
				if (collector) {
					counters_[c.alias] = collector;
					pdh.addCounter(c.path, collector);
				} else {
					NSC_LOG_ERROR_WA("Failed to load counter: " + c.alias + " = ",  c.path);
				}
			} catch (...) {
				NSC_LOG_ERROR_WA("EXCEPTION: Failed to load counter: " + c.alias + " = ", c.path);
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
					pdh.gatherData();
				} catch (const std::exception &e) {
					if (first) {	// If this is the first run an error will be thrown since the data is not yet available
						// This is "ok" but perhaps another solution would be better, but this works :)
						first = false;
					} else {
						errors.push_back("Failed to query performance counters: " + utf8::utf8_from_native(e.what()));
					}
				} catch (...) {
					errors.push_back("Failed to query performance counters: ");
				}
			} 
		}
		BOOST_FOREACH(const std::string &s, errors) {
			NSC_LOG_ERROR(s);
		}
	} while (((waitStatus = WaitForSingleObject(stop_event_, check_intervall_*100)) == WAIT_TIMEOUT));
	if (waitStatus != WAIT_OBJECT_0) {
		NSC_LOG_ERROR("Something odd happened when terminating PDH collection thread!");
		return;
	}

	{
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

__int64 PDHCollector::get_int_value(std::string counter) {
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex for: " + counter);
		return 0;
	}

	counter_map::iterator it = counters_.find(counter);
	if (it == counters_.end())
		return 0;
	collector_ptr ptr = (*it).second;
	return ptr->get_int64();
}

double PDHCollector::get_avg_value(std::string counter, unsigned int delta) {
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex for: " + counter);
		return -1;
	}

	counter_map::iterator it = counters_.find(counter);
	if (it == counters_.end()) {
		NSC_LOG_ERROR("Counter not found: " + counter);
		return -1;
	}
	collector_ptr ptr = (*it).second;
	if (!ptr) {
		NSC_LOG_ERROR("Invalid counter: " + counter);
		return -1;
	}

	return ptr->get_average(delta);
}


/**
* Get the average CPU usage for "time"
* @param time Time to check 
* @return average CPU usage
*/
int PDHCollector::getCPUAvrage(std::string time) {
	int frequency;
	{
		boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!readLock.owns_lock()) {
			NSC_LOG_ERROR("Failed to get Mutex!");
			return -1;
		}
		frequency = check_intervall_*100;

	}
	if (frequency == 0) {
		NSC_LOG_ERROR("Invalid frequency: " + strEx::s::xtos(frequency));
		return -1;
	}
	try {
		unsigned int mseconds = strEx::stoui_as_time(time);
		return static_cast<int>(get_avg_value(PDH_SYSTEM_KEY_CPU, mseconds/frequency));

	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to get CPU value: ", e);
		return -1;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to get CPU value");
		return -1;
	}
}
/**
* Get uptime from counter
* @bug Do we need to collect this all the time ? (perhaps we can collect this in real time ?)
* @return uptime for the system
* @bug Are we overflow protected here ? (seem to recall some issues with overflow before ?)
*/
long long PDHCollector::getUptime() {
	try {
		return get_int_value(PDH_SYSTEM_KEY_UPT);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to get UPTIME value: ", e);
		return -1;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to get UPTIME value");
		return -1;
	}
}
/**
* Memory commit limit (your guess is as good as mine to what this is :)
* @return Some form of memory check
*/
unsigned long long PDHCollector::getMemCommitLimit() {
	try {
		return get_int_value(PDH_SYSTEM_KEY_MCL);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to get MEM_CMT_LIMIT value: ", e);
		return -1;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to get MEM_CMT_LIMIT value");
		return -1;
	}
}
/**
*
* Memory committed bytes (your guess is as good as mine to what this is :)
* @return Some form of memory check
*/
unsigned long long PDHCollector::getMemCommit() {
	try {
		return get_int_value(PDH_SYSTEM_KEY_MCB);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to get MEM_CMT value: ", e);
		return -1;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to get MEM_CMT value");
		return -1;
	}
}

double PDHCollector::get_double(std::string counter) {
	boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!readLock.owns_lock()) {
		NSC_LOG_ERROR("Failed to get Mutex!");
		return -1;
	}
	try {
		counter_map::iterator it = counters_.find(counter);
		if (it == counters_.end()) {
			NSC_LOG_ERROR("Counter not found: " + counter);
			return -1;
		}
		return (*it).second->get_double();
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to get double value: ", e);
		return -1;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to get double value");
		return -1;
	}
}

void PDHCollector::start(boost::shared_ptr<system_counter_data> data)
{
	if (thread_)
		return;
	thread_data_ = data;
	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("PDHCollectorShutdown"));
	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&PDHCollector::thread_proc, this)));
}

bool PDHCollector::stop()
{
	SetEvent(stop_event_);
	if (thread_)
		return thread_->timed_join(boost::posix_time::seconds(5));
	return true;
}
