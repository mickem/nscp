// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/facts.hpp>
#include <string>

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
        out += "\n" + indent + "- " + (id != nullptr && id->has_string_value() ? id->string_value() : std::string("(no id)"));
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

}  // namespace client
