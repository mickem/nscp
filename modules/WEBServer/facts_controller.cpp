// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts_controller.hpp"

#include <boost/json.hpp>
#include <nscapi/protobuf/facts.hpp>
#include <utility>

#include "facts_listing.hpp"

namespace json = boost::json;

facts_controller::facts_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                   const unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/facts" : "/api/v2/facts"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &facts_controller::get_facts);
  addRoute("GET", "/sets/?$", this, &facts_controller::get_sets);
  addRoute("POST", "/commands/refresh/?$", this, &facts_controller::refresh_facts);
}

namespace {
// The envelope the core hands back, rendered for the browser. The document
// itself is converted by the shared tree renderer rather than walked here, so
// the bytes the UI sees are the bytes `nscp test facts` and the fleet upload
// agree on.
std::string render(const std::string &envelope) {
  PB::Facts::FactsResponseMessage message;
  if (envelope.empty() || !message.ParseFromString(envelope) || message.payload_size() == 0) {
    // A core that does not know the call, or one that answered with nothing.
    // An empty inventory is the honest answer and the same shape a fresh
    // install produces, so the UI needs no second code path for it.
    return R"({"revision":0,"collected":"","enabled":[],"errors":{},"gathered":{},"found":false,"facts":{}})";
  }
  const PB::Facts::FactsResponseMessage::Response &payload = message.payload(0);

  json::object out;
  out["revision"] = payload.revision();
  out["collected"] = payload.collected();
  out["path"] = payload.path();
  // The core sets `found` to answer a *path*; with no path it stays false even
  // when the document is there. For the root the honest answer is "is there a
  // document", or a UI that trusts the flag renders an empty inventory as a
  // missing one.
  out["found"] = payload.path().empty() ? payload.has_facts() : payload.found();

  json::array enabled;
  for (const std::string &id : payload.enabled()) enabled.emplace_back(id);
  out["enabled"] = enabled;

  // Per-set collection errors, as an object keyed by set id: a set that is
  // enabled and currently failing keeps its last value in `facts`, so the UI
  // has to be able to say "this is stale, and here is why".
  json::object errors;
  for (const PB::Common::KeyValue &error : payload.errors()) errors[error.key()] = error.value();
  out["errors"] = errors;

  // When each set's values were read off the machine, keyed by set id. This,
  // not `collected`, is what a reader should be shown: a producer that caches
  // hands the same values to every round, and `collected` would age them by
  // the hour while the numbers themselves are as old as the boot.
  json::object gathered;
  for (const PB::Common::KeyValue &when : payload.gathered()) gathered[when.key()] = when.value();
  out["gathered"] = gathered;

  // `facts` is a Value, not an Object: a path may address any node, so
  // `?path=os` is an object and `?path=os.family` a bare string.
  const std::string document = payload.has_facts() ? nscapi::facts::tree::to_json(payload.facts()) : "{}";
  boost::system::error_code ec;
  json::value parsed = json::parse(document, ec);
  out["facts"] = ec ? json::value(json::object()) : parsed;

  // Only on an answer that was collected on demand, so every answer read out
  // of the stored document is the bytes it was before on-demand sets existed.
  if (payload.on_demand()) out["on_demand"] = true;

  return json::serialize(out);
}

// The catalog: which sets this agent has, without their contents.
std::string render_sets(const std::string &envelope) {
  PB::Facts::FactsResponseMessage message;
  json::object out;
  json::array scheduled;
  json::array on_demand;
  if (!envelope.empty() && message.ParseFromString(envelope) && message.payload_size() > 0) {
    const PB::Facts::FactsResponseMessage::Response &payload = message.payload(0);
    for (const std::string &id : payload.enabled()) scheduled.emplace_back(id);
    for (const PB::Facts::FactsResponseMessage::Response::OnDemandSet &set : payload.on_demand_sets()) {
      json::object entry;
      entry["id"] = set.id();
      entry["parameterized"] = set.parameterized();
      // The grant that reads it: per set, never the document's `facts.get`.
      entry["privilege"] = "facts.get." + facts_listing::set_of(set.id());
      on_demand.push_back(entry);
    }
  }
  out["scheduled"] = scheduled;
  out["on_demand"] = on_demand;
  return json::serialize(out);
}
}  // namespace

void facts_controller::get_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  const std::string path = request.get("path", "");
  // The grant that reads one set and nothing else. Empty for the whole
  // document, and for a path whose first segment could not name a set.
  const std::string set = facts_listing::set_of(path);
  if (set.empty()) {
    if (!session->is_logged_in("facts.get", request, response)) return;
    response.append(render(core->get_facts(path)));
    return;
  }

  // `facts.get` reads the stored document, as it always has. `facts.get.<set>`
  // reads one set - which is how a role is given `os` without the software
  // list, and the only grant that reads an on-demand set: a script listing or
  // a counter catalog is not something a role was handed along with the
  // inventory, so `facts.get` alone does not reach it.
  const std::string per_set = "facts.get." + set;
  std::string granted;
  if (!session->is_logged_in(session_manager_interface::grant_options{per_set, "facts.get"}, request, response, &granted)) return;
  if (granted == per_set) {
    response.append(render(core->get_facts(path)));
    return;
  }
  // The document's grant: read without collecting anything, so a caller who
  // may not read an on-demand set cannot make the agent enumerate it either.
  const std::string envelope = core->get_facts(path, nscapi::core_wrapper::facts_read::stored_only);
  if (facts_listing::parse(envelope, set).on_demand) {
    response.setCodeForbidden("403 The on-demand fact set '" + set + "' requires the " + per_set + " privilege");
    return;
  }
  response.append(render(envelope));
}

void facts_controller::get_sets(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  // Names only, so the document's grant covers it: it says no more than
  // `enabled` already does.
  if (!session->is_logged_in("facts.get", request, response)) return;
  response.append(render_sets(core->get_fact_sets()));
}

void facts_controller::refresh_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  // A refresh asks every loaded producer to collect now, which is work an
  // anonymous or read-only caller should not be able to trigger on a loop -
  // hence its own grant rather than riding on facts.get.
  if (!session->is_logged_in("facts.refresh", request, response)) return;
  if (!core->refresh_facts()) {
    response.setCodeServerError("Failed to refresh facts");
    return;
  }
  // Answer with the document the round produced, so the caller does not have
  // to follow up with a GET to see what changed.
  response.append(render(core->get_facts("")));
}
