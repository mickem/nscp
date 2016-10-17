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

#include <boost/thread/shared_mutex.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

class SimpleCache : public nscapi::impl::simple_plugin {
public:
	struct cache_query {
		std::string channel;
		std::string host;
		std::string alias;
		std::string command;
	};
private:
	typedef boost::function<std::string(const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload)> index_lookup_function;
	typedef boost::function<std::string(const cache_query &query)> command_lookup_function;
	typedef std::list<index_lookup_function> index_lookup_type;
	typedef std::list<command_lookup_function> command_lookup_type;
	index_lookup_type index_lookup_;
	command_lookup_type command_lookup_;

	std::map<std::string, std::string> cache_;
	boost::shared_mutex cache_mutex_;

public:
	SimpleCache() {}
	virtual ~SimpleCache() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);

	void handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);
	void check_cache(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};
