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

#include "pdh_thread.hpp"
#include <CheckMemory.h>

#include "check_pdh.hpp"

#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_metrics.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

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
	void check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void check_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void check_pdh(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void check_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void check_pagefile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void add_counter(std::string key, std::string query);
	void check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void check_network(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

	// Metrics
	void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

	// Legacy checks
	void checkCpu(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void checkMem(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void checkUptime(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void checkServiceState(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void checkProcState(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
	void checkCounter(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
