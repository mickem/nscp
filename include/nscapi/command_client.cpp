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

#include <nscapi/command_client.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/command_proxy.hpp>

#include <boost/foreach.hpp>

namespace nscapi {
	namespace command_helper {
		void register_command_helper::add(boost::shared_ptr<command_info> d) {
			owner->add(d);
		}
		void add_metadata_helper::add(std::string key, std::string value) {
			owner->set(key, value);
		}

		void command_registry::register_all() {
			if (commands.size() == 0)
				return;
			Plugin::RegistryRequestMessage request;
			BOOST_FOREACH(command_list::value_type v, commands) {
				Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
				Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
				regitem->set_plugin_id(core_->get_plugin_id());
				regitem->set_type(Plugin::Registry_ItemType_QUERY);
				regitem->set_name(v->name);
				regitem->mutable_info()->set_title(v->name);
				regitem->mutable_info()->set_description(v->description);
				BOOST_FOREACH(const std::string &alias, v->aliases) {
					regitem->add_alias(alias);
				}
			}
			std::string response_string;
			core_->registry_query(request.SerializeAsString(), response_string);
			Plugin::RegistryResponseMessage response;
			response.ParseFromString(response_string);
			for (int i = 0; i < response.payload_size(); i++) {
				if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK) {
					errors.push_back(response.payload(i).result().message());
				}
			}
		}
	}
}