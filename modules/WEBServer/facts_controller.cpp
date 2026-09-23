// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "facts_controller.hpp"

#include <boost/json.hpp>
#include <nscapi/protobuf/facts.hpp>
#include <utility>

namespace json = boost::json;

facts_controller::facts_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                   const unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/facts" : "/api/v2/facts"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &facts_controller::get_facts);
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
    return R"({"revision":0,"collected":"","enabled":[],"errors":{},"found":false,"facts":{}})";
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

  // `facts` is a Value, not an Object: a path may address any node, so
  // `?path=os` is an object and `?path=os.family` a bare string.
  const std::string document = payload.has_facts() ? nscapi::facts::tree::to_json(payload.facts()) : "{}";
  boost::system::error_code ec;
  json::value parsed = json::parse(document, ec);
  out["facts"] = ec ? json::value(json::object()) : parsed;

  return json::serialize(out);
}
}  // namespace

void facts_controller::get_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("facts.get", request, response)) return;
  response.append(render(core->get_facts(request.get("path", ""))));
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
