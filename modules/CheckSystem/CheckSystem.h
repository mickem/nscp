/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "pdh_thread.hpp"
#include <CheckMemory.h>

#include "check_pdh.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_settings_object.hpp>

#include "filter_config_object.hpp"

class CheckSystem : public nscapi::impl::simple_plugin {
private:
	CheckMemory memoryChecker;
	boost::shared_ptr<pdh_thread> collector;

	typedef std::map<std::string, std::string> counter_map_type;
	counter_map_type counters;

	//	std::map<DWORD,std::string> lookups_;

	check_pdh::check pdh_checker;

public:
	CheckSystem() {}
	virtual ~CheckSystem() {}

	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	NSCAPI::nagiosReturn commandLineExec(const int target_mode, const std::string &command, const std::list<std::string> &arguments, std::string &result);

	// Checks
	void check_service(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_memory(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_pdh(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_process(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_cpu(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_uptime(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_pagefile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void add_counter(std::string key, std::string query);
	void check_os_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_network(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);

	// Metrics
	void fetchMetrics(Plugin::MetricsMessage::Response *response);

	// Legacy checks
	void checkCpu(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkMem(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkUptime(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkServiceState(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkProcState(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkCounter(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};
