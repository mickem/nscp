// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_sources.hpp"

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/protobuf/registry.hpp>

namespace {
// One inventory request for queries, by name or (with an empty name) all of
// them. Never fetch_all: that makes the core run every command with
// `help-pb` to collect its parameters, and nothing here reads them.
std::vector<ncpa_sources::query_info> inventory(const nscapi::core_wrapper *core, const std::string &name) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  if (!name.empty()) payload->mutable_inventory()->set_name(name);
  payload->mutable_inventory()->set_fetch_all(false);
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  std::vector<ncpa_sources::query_info> out;
  for (const PB::Registry::RegistryResponseMessage::Response &r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
      ncpa_sources::query_info info;
      info.name = i.name();
      if (i.info().plugin_size() > 0) info.owner = i.info().plugin(0);
      out.push_back(info);
    }
  }
  return out;
}
}  // namespace

std::vector<ncpa_sources::query_info> ncpa_sources::list_queries() const { return inventory(core_, ""); }

bool ncpa_sources::describe_query(const std::string &name, query_info &out) const {
  if (name.empty()) return false;
  for (const query_info &info : inventory(core_, name)) {
    out = info;
    return true;
  }
  return false;
}

int ncpa_sources::run_query(const std::string &name, const std::list<std::string> &arguments, std::string &message, std::string &perf) const {
  nscapi::core_helper ch(core_, static_cast<int>(plugin_id_));
  // No length cap: NCPA has no payload limit, so the whole output goes back.
  return ch.simple_query(name, arguments, message, perf, static_cast<std::size_t>(-1));
}
