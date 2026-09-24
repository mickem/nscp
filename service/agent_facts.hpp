// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <nscapi/protobuf/facts.hpp>
#include <string>
#include <vector>

namespace nsclient {
namespace core {

// The `agent` fact set: what NSClient++ itself is on this host - its version,
// the modules it has loaded and whether it is enrolled with a fleet server.
//
// The one set the core produces rather than a module, because none of it is a
// module's to know. It is enabled in the core's own [/settings/facts] section
// for the same reason every other set is enabled in its producer's section:
// the switch lives with whoever does the work.
//
// Never the identity: `enrolled` says yes or no, and nothing about which
// server, which certificate or which host id - the fleet server knows those
// already, and nothing else should learn them from an inventory.
namespace agent_facts {

constexpr const char *const set_agent = "agent";

// The set's document. `modules` is sorted and de-duplicated here: a module
// loaded twice under two aliases is one module, and the order modules happen
// to load in is not a change worth a revision.
inline PB::Facts::Object build(const std::string &version, std::vector<std::string> modules, const bool enrolled) {
  PB::Facts::Object object;
  if (!version.empty()) {
    PB::Facts::Field *field = object.add_fields();
    field->set_key("version");
    field->mutable_value()->set_string_value(version);
  }
  std::sort(modules.begin(), modules.end());
  modules.erase(std::unique(modules.begin(), modules.end()), modules.end());
  modules.erase(std::remove(modules.begin(), modules.end(), std::string()), modules.end());
  PB::Facts::Field *list = object.add_fields();
  list->set_key("modules");
  PB::Facts::List *values = list->mutable_value()->mutable_list_value();
  for (const std::string &module : modules) values->add_values()->set_string_value(module);
  PB::Facts::Field *field = object.add_fields();
  field->set_key("enrolled");
  field->mutable_value()->set_bool_value(enrolled);
  return object;
}

}  // namespace agent_facts
}  // namespace core
}  // namespace nsclient
