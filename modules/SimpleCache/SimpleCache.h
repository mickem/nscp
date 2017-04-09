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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/function.hpp>


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

	typedef std::map<std::string, std::string> cache_type;
	cache_type cache_;
	boost::shared_mutex cache_mutex_;

public:
	SimpleCache() {}
	virtual ~SimpleCache() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);

	void handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);
	void check_cache(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	void list_cache(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};
