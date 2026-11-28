// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "metadata_controller.hpp"

#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <nscapi/protobuf/registry.hpp>
#include <utility>

#include "facts_listing.hpp"
#include "str/constant_time.hpp"
#include "str/xtos.hpp"

namespace json = boost::json;

metadata_controller::metadata_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                         unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/metadata" : "/api/v2/metadata"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &metadata_controller::get_index);
  addRoute("GET", "/counters/?$", this, &metadata_controller::get_counters);
  addRoute("GET", "/channels/?$", this, &metadata_controller::get_channels);
}

void metadata_controller::get_index(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metadata.list", request, response)) return;

  json::array root;
  json::object counters;
  counters["name"] = "counters";
  counters["title"] = "Performance counters";
  counters["url"] = get_base(request) + "/counters";
  root.push_back(counters);
  json::object channels;
  channels["name"] = "channels";
  channels["title"] = "Registered submission channels";
  channels["url"] = get_base(request) + "/channels";
  root.push_back(channels);
  response.append(json::serialize(root));
}

void metadata_controller::get_counters(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metadata.get.counters", request, response)) return;

  // The catalog is CheckSystem's on-demand `pdh.objects` fact set. It used to
  // be the stdout of `CheckSystem pdh --list --json --all --no-instances`,
  // run through exec; the body is the same JSON array of `\object\counter`
  // paths. Enumerating it is the expensive part, so this is content with the
  // core's few seconds of cache.
  const facts_listing::reading reading = facts_listing::parse(core->get_facts("pdh.objects"), "pdh");
  if (!reading.found) {
    if (!reading.error.empty()) {
      response.setCodeServerError("Command returned errors: " + reading.error);
    } else {
      response.setCodeServerError("No response from module, is the CheckSystem module loaded?");
    }
    return;
  }
  const std::string message = facts_listing::render_counters(reading.value);
  response.get_headers()["XX-debug"] = "size: " + str::xtos(message.size());

  response.get_headers()["Content-Type"] = "application/json";
  response.append(message);
}

void metadata_controller::get_channels(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metadata.get.channels", request, response)) return;

  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::HANDLER);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  json::array root;
  for (const PB::Registry::RegistryResponseMessage::Response &r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
      json::object node;
      node["name"] = i.name();
      json::array plugins;
      for (const std::string &plugin : i.info().plugin()) {
        plugins.push_back(json::value(plugin));
      }
      node["plugins"] = plugins;
      root.push_back(node);
    }
  }
  response.get_headers()["Content-Type"] = "application/json";
  response.append(json::serialize(root));
}
