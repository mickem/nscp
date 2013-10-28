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

//#include <pdh.hpp>
#include "pdh_thread.hpp"
#include <CheckMemory.h>
#include <protobuf/plugin.pb.h>

#include "check_pdh.hpp"


#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_settings_object.hpp>

#include "filter_config_object.hpp"

template<class T>
inline void import_string(T &object, T &parent) {
	if (object.empty() && !parent.empty())
		object = parent;
}
class CheckSystem : public nscapi::impl::simple_plugin {
private:
	CheckMemory memoryChecker;
	pdh_thread collector;

	typedef std::map<std::string,std::string> counter_map_type;
	counter_map_type counters;

//	std::map<DWORD,std::string> lookups_;

	check_pdh::check pdh_checker;

public:
	CheckSystem() {}
	virtual ~CheckSystem() {}

	virtual bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	virtual bool unloadModule();

	NSCAPI::nagiosReturn commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result);

	void check_service(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_memory(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_pdh(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_process(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_cpu(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_uptime(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_pagefile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void add_counter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string path, std::string key, std::string query);
	void check_os_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);


	// Legacy checks
	void checkCpu(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkMem(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkUptime(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkServiceState(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkProcState(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void checkCounter(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);


};
