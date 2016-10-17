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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_settings_object.hpp>

#include "filter_config_object.hpp"

class CheckSystem : public nscapi::impl::simple_plugin {
public:
	CheckSystem() {}
	virtual ~CheckSystem() {}

	virtual bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	virtual bool unloadModule();

	void check_service(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_memory(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	//void check_pdh(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_process(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_cpu(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_uptime(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void check_pagefile(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void add_counter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string path, std::string key, std::string query);
	void check_os_version(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};
