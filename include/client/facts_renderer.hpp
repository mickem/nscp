// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <map>
#include <nscapi/protobuf/facts.hpp>
#include <set>
#include <string>
#include <vector>

// Rendering the host inventory for the `nscp test` prompt.
//
// The core answers a facts query with a FactsResponseMessage: the document (or
// the subtree asked for) plus the revision, the fact sets the producers say
// they are collecting and anything that failed to collect. The prompt is read
// by a person, so the tree is rendered indented - one line per scalar, a
// record's `id` first so a list reads as a list of things.
//
// A header of its own, rather than a few statics in simple_client.cpp, because
// it is a pure function from the core's answer to the text on screen and is
// worth testing as one.
namespace client {
namespace facts_detail {

inline void render_value(const PB::Facts::Value &value, const std::string &indent, std::string &out);

inline void render_object(const PB::Facts::Object &object, const std::string &indent, std::string &out) {
  for (const PB::Facts::Field &field : object.fields()) {
    if (field.value().has_object_value() || field.value().has_list_value()) {
      out += "\n" + indent + field.key() + ":";
      render_value(field.value(), indent + "  ", out);
    } else {
      out += "\n" + indent + field.key() + ": ";
      render_value(field.value(), indent, out);
    }
  }
}

inline void render_value(const PB::Facts::Value &value, const std::string &indent, std::string &out) {
  switch (value.kind_case()) {
    case PB::Facts::Value::kObjectValue:
      render_object(value.object_value(), indent, out);
      return;
    case PB::Facts::Value::kListValue: {
      if (value.list_value().values_size() == 0) {
        out += " (none)";
        return;
      }
      for (const PB::Facts::Value &entry : value.list_value().values()) {
        if (!entry.has_object_value()) {
          out += "\n" + indent + "- ";
          render_value(entry, indent, out);
          continue;
        }
        // A record's id is what names it, so it leads the entry and the rest
        // of its fields hang under it.
        const PB::Facts::Value *id = nscapi::facts::tree::get(entry.object_value(), "id");
        out += "\n" + indent + "- " + (id != nullptr && id->kind_case() == PB::Facts::Value::kStringValue ? id->string_value() : std::string("(no id)"));
        PB::Facts::Object rest;
        for (const PB::Facts::Field &field : entry.object_value().fields()) {
          if (field.key() == "id") continue;
          *rest.add_fields() = field;
        }
        render_object(rest, indent + "    ", out);
      }
      return;
    }
    case PB::Facts::Value::kStringValue:
      out += value.string_value();
      return;
    case PB::Facts::Value::kIntValue:
      out += std::to_string(value.int_value());
      return;
    case PB::Facts::Value::kUintValue:
      out += std::to_string(value.uint_value());
      return;
    case PB::Facts::Value::kDoubleValue:
      out += nscapi::facts::tree::format_double(value.double_value());
      return;
    case PB::Facts::Value::kBoolValue:
      out += value.bool_value() ? "true" : "false";
      return;
    default:
      out += "(unset)";
      return;
  }
}

}  // namespace facts_detail

// `body` is what core_wrapper::get_facts() returned, and `path` the dotted
// path that was asked for (empty for the whole document).
inline std::string render_facts(const std::string &body, const std::string &path) {
  PB::Facts::FactsResponseMessage message;
  // An empty answer is a core that does not know the call; anything else that
  // will not parse is a truncated buffer. Neither is something the prompt can
  // do anything about beyond saying so.
  if (body.empty()) return "This core does not serve facts.";
  if (!message.ParseFromString(body) || message.payload_size() == 0) return "Could not read the facts the core returned.";
  const PB::Facts::FactsResponseMessage::Response &payload = message.payload(0);
  if (payload.result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
    return "Could not read the facts: " + (payload.result().message().empty() ? std::string("the core reported an error") : payload.result().message());
  }

  // A path the document does not have is not an error at the core, so the
  // `found` flag is what separates "nothing produced it" from an empty subtree.
  if (!path.empty() && !payload.found()) return "No facts at: " + path;

  std::string out = "Revision: " + std::to_string(payload.revision());
  // "Checked" rather than "Collected": this is when the core last asked, and
  // for a producer that caches its snapshot that is not when the values were
  // read. Each set says that for itself, below.
  if (!payload.collected().empty()) out += "  Checked: " + payload.collected();

  std::string names;
  for (const std::string &id : payload.enabled()) {
    if (!names.empty()) names += ", ";
    names += id;
  }
  // Nothing enabled is the default state, so it says what to do about it
  // rather than printing an empty line. Which sets exist is the producing
  // module's own configuration, which is also where they are turned on.
  out +=
      "\nEnabled: " +
      (names.empty() ? std::string("(none - a fact set is enabled in the module that produces it, e.g. `[/settings/system/windows/facts] os = true`)") : names);

  if (payload.gathered_size() > 0) {
    // The age of the values themselves, per set. A set whose producer reads
    // on every round shows the round's time here; one that caches shows when
    // it actually looked.
    out += "\nGathered:";
    for (const PB::Common::KeyValue &entry : payload.gathered()) out += "\n  " + entry.key() + ": " + entry.value();
  }

  if (payload.errors_size() > 0) {
    out += "\nErrors:";
    for (const PB::Common::KeyValue &entry : payload.errors()) out += "\n  " + entry.key() + ": " + entry.value();
  }

  if (!payload.has_facts()) return out;
  const PB::Facts::Value &facts = payload.facts();
  if (facts.has_object_value() && facts.object_value().fields_size() == 0) return out + "\n\n(no facts collected)";
  out += "\n";
  if (!path.empty()) {
    // A path may address a scalar (`facts os.family`), which belongs on the
    // same line as the path rather than under it like a subtree's contents.
    out += "\n" + path + ":";
    if (!facts.has_object_value() && !facts.has_list_value()) out += " ";
  }
  facts_detail::render_value(facts, "  ", out);
  return out;
}

// One fact-set switch as the settings registry describes it: a bool key in a
// section called `facts`, registered by the module that produces the set (or
// by the core, for `agent`). The key is the set's own dotted id.
struct fact_set_switch {
  std::string id;
  std::string producer;  // the registering module, empty for the core
  std::string section;   // e.g. /settings/system/unix/facts
  bool configured = false;
};

// `facts list`: every set this agent can produce, whether the configuration
// turns it on, and whether the core's last round agrees.
//
// The two can differ, and that is the line worth reading: a switch takes
// effect when the module re-reads its configuration, so a set switched on in
// the file but not reloaded yet is not being collected, and one switched off
// is still in the document until then. `facts_body` is what
// core_wrapper::get_facts() returned; empty or unreadable, the column falls
// back to the configuration alone.
inline std::string render_fact_sets(std::vector<fact_set_switch> sets, const std::string &facts_body) {
  if (sets.empty()) {
    return "No module that produces facts is loaded. Fact sets come from CheckSystem, CheckDisk, CheckDocker, CheckTaskSched and others; load one "
           "and run `facts list` again.";
  }

  std::set<std::string> claimed;
  std::map<std::string, std::string> errors;
  bool have_round = false;
  PB::Facts::FactsResponseMessage message;
  if (!facts_body.empty() && message.ParseFromString(facts_body) && message.payload_size() > 0 &&
      message.payload(0).result().code() == PB::Common::Result_StatusCodeType_STATUS_OK) {
    have_round = true;
    for (const std::string &id : message.payload(0).enabled()) claimed.insert(id);
    for (const PB::Common::KeyValue &error : message.payload(0).errors()) errors[error.key()] = error.value();
  }

  // Grouped by producer, the way the web UI's Facts page groups them, and by
  // id within one so the list reads the same on every run.
  std::sort(sets.begin(), sets.end(), [](const fact_set_switch &a, const fact_set_switch &b) {
    if (a.producer != b.producer) return a.producer < b.producer;
    return a.id < b.id;
  });

  std::vector<std::vector<std::string>> rows;
  rows.push_back({"SET", "STATE", "PRODUCER", "SECTION"});
  for (const fact_set_switch &set : sets) {
    std::string state;
    const bool collecting = claimed.count(set.id) > 0;
    if (!have_round) {
      state = set.configured ? "enabled" : "disabled";
    } else if (set.configured && collecting) {
      state = errors.count(set.id) > 0 ? "enabled, failing: " + errors[set.id] : "enabled";
    } else if (set.configured) {
      state = "enabled (reload to start)";
    } else if (collecting) {
      state = "disabled (reload to stop)";
    } else {
      state = "disabled";
    }
    rows.push_back({set.id, state, set.producer.empty() ? std::string("core") : set.producer, set.section});
  }

  // Aligned columns, two spaces apart; the last column is not padded.
  std::vector<std::size_t> widths(4, 0);
  for (const std::vector<std::string> &row : rows) {
    for (std::size_t i = 0; i < row.size(); ++i) widths[i] = std::max(widths[i], row[i].size());
  }
  std::string out;
  for (const std::vector<std::string> &row : rows) {
    if (!out.empty()) out += "\n";
    for (std::size_t i = 0; i < row.size(); ++i) {
      out += row[i];
      if (i + 1 < row.size()) out += std::string(widths[i] - row[i].size() + 2, ' ');
    }
  }
  return out;
}

}  // namespace client
