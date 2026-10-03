// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_sources.hpp"

#include <map>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/protobuf/registry.hpp>

namespace {
PB::Registry::RegistryResponseMessage registry_inventory(const nscapi::core_wrapper *core, const std::string &name,
                                                         const std::vector<PB::Registry::ItemType> &types) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  if (!name.empty()) payload->mutable_inventory()->set_name(name);
  // Never fetch_all: for queries that makes the core run every command with
  // `help-pb` to collect its parameters, and for modules it lists every module
  // on disk. Nothing here needs either.
  payload->mutable_inventory()->set_fetch_all(false);
  for (const PB::Registry::ItemType type : types) payload->mutable_inventory()->add_type(type);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);
  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  return pb_response;
}

// The registry names a query's owner by the alias its module was loaded
// under, or by the module name when it has none. The loaded modules map that
// back to the module name (the inventory's id is the alias-or-name).
std::map<std::string, std::string> module_names(const nscapi::core_wrapper *core) {
  std::map<std::string, std::string> out;
  // Held in a local: a range-for over a temporary's payload() would iterate
  // freed memory.
  const PB::Registry::RegistryResponseMessage response = registry_inventory(core, "", {PB::Registry::ItemType::MODULE});
  for (const auto &r : response.payload()) {
    for (const auto &i : r.inventory()) {
      out[i.id()] = i.name();
    }
  }
  return out;
}

std::vector<ncpa_sources::query_info> queries(const nscapi::core_wrapper *core, const std::string &name) {
  std::vector<PB::Registry::ItemType> types{PB::Registry::ItemType::QUERY};
  // The registry lists the aliases separately; a lookup by name finds both.
  if (name.empty()) types.push_back(PB::Registry::ItemType::QUERY_ALIAS);
  const PB::Registry::RegistryResponseMessage response = registry_inventory(core, name, types);
  const std::map<std::string, std::string> modules = module_names(core);
  std::vector<ncpa_sources::query_info> out;
  for (const auto &r : response.payload()) {
    for (const auto &i : r.inventory()) {
      ncpa_sources::query_info info;
      info.name = i.name();
      if (i.info().plugin_size() > 0) {
        const auto it = modules.find(i.info().plugin(0));
        info.module = it == modules.end() ? i.info().plugin(0) : it->second;
      }
      out.push_back(info);
    }
  }
  return out;
}
}  // namespace

std::vector<ncpa_sources::query_info> ncpa_sources::list_queries() const { return queries(core_, ""); }

bool ncpa_sources::describe_query(const std::string &name, query_info &out) const {
  if (name.empty()) return false;
  for (const query_info &info : queries(core_, name)) {
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
