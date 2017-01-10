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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/function.hpp>

struct config_object {
	std::string time_format;
};

class SimpleFileWriter : public nscapi::impl::simple_plugin {
public:
	typedef boost::function<std::string(const config_object &config, const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload)> index_lookup_function;
	typedef std::list<index_lookup_function> index_lookup_type;
private:
	index_lookup_type syntax_service_lookup_, syntax_host_lookup_;
	std::string filename_;
	boost::shared_mutex cache_mutex_;
	config_object config_;

public:
	SimpleFileWriter() {}
	virtual ~SimpleFileWriter() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	void handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);
};
