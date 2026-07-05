// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "events_controller.hpp"

#include <boost/json.hpp>

namespace json = boost::json;

namespace {
json::object to_json(const event_store::event_entry &e) {
  json::object node;
  node.insert(json::object::value_type("index", static_cast<std::int64_t>(e.index)));
  node.insert(json::object::value_type("event", e.event));
  node.insert(json::object::value_type("date", e.date));
  json::object data;
  for (const auto &kv : e.data) {
    data.insert(json::object::value_type(kv.first, kv.second));
  }
  node.insert(json::object::value_type("data", data));
  return node;
}

json::array to_json(const event_store::event_list &list) {
  json::array root;
  for (const event_store::event_entry &e : list) {
    root.push_back(to_json(e));
  }
  return root;
}
}  // namespace

events_controller::events_controller(int version, const std::shared_ptr<session_manager_interface> &session, const std::shared_ptr<event_store> &events)
    : RegexpController(version == 1 ? "/api/v1/events" : "/api/v2/events"), session(session), events(events) {
  addRoute("GET", "/?$", this, &events_controller::list_events);
  addRoute("DELETE", "/?$", this, &events_controller::clear_events);
}

void events_controller::list_events(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("events.list", request, response)) return;
  response.append(json::serialize(to_json(events->list())));
}

void events_controller::clear_events(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("events.delete", request, response)) return;
  // Get-and-clear: callers receive the events that were buffered, and the
  // store is left empty so the next caller does not see them again. This
  // turns the endpoint into a drain queue (consumer pulls + acks in one
  // hop) rather than requiring a separate clear after read.
  response.append(json::serialize(to_json(events->pop_all())));
}
