// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <nscapi/protobuf/facts.hpp>
#include <string>

// Reading listings out of the facts the core serves, for the controllers that
// used to get them by running a module's command line.
//
// `GET /api/v2/scripts/<runtime>` and `GET /api/v2/metadata/counters` each
// built an ExecuteRequest (`list --json`, `pdh --list --json --all
// --no-instances`) and passed the module's stdout through. Those listings are
// on-demand fact sets now (`scripts.<runtime>`, `pdh.objects`), so the
// controllers read them there and render the same JSON the command printed.
//
// Pure functions from the core's answer to the response body, so the byte
// compatibility of the two old endpoints is something a unit test holds.
namespace facts_listing {

// The first segment of a facts path when it can be the last segment of a
// grant (`facts.get.<set>`), and empty otherwise. The same rule a fact set
// name follows - which also keeps a path such as `*` or `a,b` from ever being
// spliced into a grant string.
inline std::string set_of(const std::string &path) {
  const std::string head = path.substr(0, path.find('.'));
  if (head.empty() || head.size() > 64) return "";
  if (head[0] < 'a' || head[0] > 'z') return "";
  for (const char c : head) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!ok) return "";
  }
  return head;
}

// What the core answered for one path.
struct reading {
  // The core answered at all (false: a core without the call).
  bool answered = false;
  bool found = false;
  bool on_demand = false;
  PB::Facts::Value value;
  // The collection error reported for `error_id` or anything below it.
  std::string error;
};

inline reading parse(const std::string &envelope, const std::string &error_id) {
  reading out;
  PB::Facts::FactsResponseMessage message;
  if (envelope.empty() || !message.ParseFromString(envelope) || message.payload_size() == 0) return out;
  const PB::Facts::FactsResponseMessage::Response &payload = message.payload(0);
  out.answered = true;
  out.found = payload.found();
  out.on_demand = payload.on_demand();
  if (payload.has_facts()) out.value = payload.facts();
  const std::string prefix = error_id + ".";
  for (const PB::Common::KeyValue &error : payload.errors()) {
    if (error.key() == error_id || error.key().compare(0, prefix.size(), prefix) == 0) out.error = error.value();
  }
  return out;
}

// Whether the core's SETS answer lists this on-demand set.
inline bool declares(const std::string &envelope, const std::string &id) {
  PB::Facts::FactsResponseMessage message;
  if (envelope.empty() || !message.ParseFromString(envelope) || message.payload_size() == 0) return false;
  for (const PB::Facts::FactsResponseMessage::Response::OnDemandSet &set : message.payload(0).on_demand_sets()) {
    if (set.id() == id) return true;
  }
  return false;
}

// A plain list of strings as the JSON array `list --json` printed, in the
// order the producer listed them.
inline std::string render_names(const PB::Facts::Value &list) {
  boost::json::array data;
  if (list.has_list_value()) {
    for (const PB::Facts::Value &item : list.list_value().values()) {
      if (item.kind_case() == PB::Facts::Value::kStringValue) data.push_back(boost::json::value(item.string_value()));
    }
  }
  return boost::json::serialize(data);
}

// `pdh.objects` as the JSON array `pdh --list --json --all --no-instances`
// printed: one `\<object>\<counter>` per counter, objects and counters in the
// order PDH enumerated them.
inline std::string render_counters(const PB::Facts::Value &objects) {
  boost::json::array data;
  if (objects.has_list_value()) {
    for (const PB::Facts::Value &item : objects.list_value().values()) {
      if (!item.has_object_value()) continue;
      const PB::Facts::Value *name = nscapi::facts::tree::get(item.object_value(), "id");
      const PB::Facts::Value *counters = nscapi::facts::tree::get(item.object_value(), "counters");
      if (name == nullptr || counters == nullptr || !counters->has_list_value()) continue;
      for (const PB::Facts::Value &counter : counters->list_value().values()) {
        if (counter.kind_case() != PB::Facts::Value::kStringValue) continue;
        data.push_back(boost::json::value("\\" + name->string_value() + "\\" + counter.string_value()));
      }
    }
  }
  return boost::json::serialize(data);
}

}  // namespace facts_listing
