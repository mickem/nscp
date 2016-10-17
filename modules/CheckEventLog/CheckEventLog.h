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

#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

struct real_time_thread;
class CheckEventLog : public nscapi::impl::simple_plugin {
private:
	boost::shared_ptr<real_time_thread> thread_;
	bool debug_;
	std::string syntax_;
	int buffer_length_;
	bool lookup_names_;

public:
	CheckEventLog() {}
	virtual ~CheckEventLog() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void parse(std::wstring expr);

	void check_eventlog(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void CheckEventLog_(Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);

	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void insert_eventlog(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	void list_providers(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
};
