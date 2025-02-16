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
#include <nscapi/nscapi_protobuf_registry.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/command_proxy.hpp>

namespace nscapi {
namespace command_helper {
void register_command_helper::add(boost::shared_ptr<command_info> d) { owner->add(d); }
void add_metadata_helper::add(std::string key, std::string value) { owner->set(key, value); }
void register_command(command_proxy_ptr &core_, PB::Registry::RegistryRequestMessage &request, std::list<boost::shared_ptr<command_info> >::value_type &v) {
  PB::Registry::RegistryRequestMessage::Request *payload = request.add_payload();
  PB::Registry::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
  regitem->set_plugin_id(core_->get_plugin_id());
  regitem->set_type(PB::Registry::ItemType::QUERY);
  regitem->set_name(v->name);
  regitem->mutable_info()->set_title(v->name);
  regitem->mutable_info()->set_description(v->description);
  for (const std::string &alias : v->aliases) {
    regitem->add_alias(alias);
  }
}

void command_registry::register_all() {
  if (commands.size() == 0) return;
  PB::Registry::RegistryRequestMessage request;
  for (command_list::value_type v : commands) {
    register_command(core_, request, v);
  }
  std::string response_string;
  core_->registry_query(request.SerializeAsString(), response_string);
  PB::Registry::RegistryResponseMessage response;
  response.ParseFromString(response_string);
  for (int i = 0; i < response.payload_size(); i++) {
    if (response.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
      errors.push_back(response.payload(i).result().message());
    }
  }
}
}  // namespace command_helper
}  // namespace nscapi