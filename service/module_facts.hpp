// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <nscapi/protobuf/facts.hpp>
#include <set>
#include <string>
#include <vector>

namespace nsclient {
namespace core {

// The `modules` fact set: one record per module the agent has loaded.
//
// `agent.modules` already names them; this is the same list with what a fleet
// server wants beside each name - which version, under which alias, and
// whether the configuration will load it again at the next start. Produced by
// the core, like `agent`, and enabled in the core's own [/settings/facts]
// section.
//
// A scheduled set, stored and uploaded like any other: it is a few dozen
// records that move only when a module is loaded or unloaded, which is
// exactly what the stored document is for.
namespace module_facts {

constexpr const char *const set_modules = "modules";

struct module {
  std::string name;   // the module: CheckSystem
  std::string alias;  // set when it is loaded under another name
  std::string title;
  std::string version;
  // Listed in [/modules], so loaded again at the next start. A module loaded
  // by hand, or by another module, is loaded without being enabled.
  bool enabled = false;
};

namespace detail {
inline void add_string(PB::Facts::Object *object, const std::string &key, const std::string &value) {
  // An unknown value is omitted, never written empty.
  if (value.empty()) return;
  PB::Facts::Field *field = object->add_fields();
  field->set_key(key);
  field->mutable_value()->set_string_value(value);
}
}  // namespace detail

// The set's document. Takes its input by value and owns every string it
// writes: nothing in the result points back into the plugin list.
//
// The record id is the alias when there is one and the module name otherwise,
// which is what /api/v2/modules calls `id` and what `load`/`unload` take.
// Sorted by id, so the order modules happen to load in is not a change worth
// a revision; a second module with an id already listed is dropped, because
// an id has to be unique in its list.
inline PB::Facts::Object build(std::vector<module> modules) {
  modules.erase(std::remove_if(modules.begin(), modules.end(), [](const module &m) { return m.name.empty(); }), modules.end());
  const auto id_of = [](const module &m) -> const std::string & { return m.alias.empty() ? m.name : m.alias; };
  std::sort(modules.begin(), modules.end(), [&id_of](const module &a, const module &b) { return id_of(a) < id_of(b); });

  PB::Facts::Object object;
  PB::Facts::Field *loaded = object.add_fields();
  loaded->set_key("loaded");
  PB::Facts::List *list = loaded->mutable_value()->mutable_list_value();
  std::set<std::string> ids;
  for (const module &m : modules) {
    if (!ids.insert(id_of(m)).second) continue;
    PB::Facts::Object *record = list->add_values()->mutable_object_value();
    detail::add_string(record, "id", id_of(m));
    detail::add_string(record, "name", m.name);
    detail::add_string(record, "alias", m.alias);
    detail::add_string(record, "title", m.title);
    detail::add_string(record, "version", m.version);
    PB::Facts::Field *enabled = record->add_fields();
    enabled->set_key("enabled");
    enabled->mutable_value()->set_bool_value(m.enabled);
  }
  return object;
}

}  // namespace module_facts
}  // namespace core
}  // namespace nsclient
