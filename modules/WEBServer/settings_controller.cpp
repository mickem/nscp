#include "settings_controller.hpp"

#include <nscapi/nscapi_protobuf_settings.hpp>

#include <boost/json.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <utility>

namespace json = boost::json;

settings_controller::settings_controller(const int version, boost::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                         unsigned int plugin_id)
    : session(std::move(session)), core(core), plugin_id(plugin_id), RegexpController(version == 1 ? "/api/v1/settings" : "/api/v2/settings") {
  addRoute("GET", "/descriptions(.*)$", this, &settings_controller::get_desc);
  addRoute("POST", "/command$", this, &settings_controller::command);
  addRoute("GET", "/status$", this, &settings_controller::status);
  addRoute("GET", "(.*)$", this, &settings_controller::get);
  addRoute("PUT", "(.*)$", this, &settings_controller::put);
}

void settings_controller::get(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("settings.get", request, response)) return;

  if (!validate_arguments(1, what, response)) {
    return;
  }

  std::string path = what.str(1);

  PB::Settings::SettingsRequestMessage rm;
  PB::Settings::SettingsRequestMessage::Request *payload = rm.add_payload();
  payload->mutable_query()->mutable_node()->set_path(path);
  payload->mutable_query()->set_recursive(true);
  payload->mutable_query()->set_include_keys(true);
  payload->set_plugin_id(plugin_id);

  std::string str_response;
  core->settings_query(rm.SerializeAsString(), str_response);
  PB::Settings::SettingsResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  if (pb_response.payload_size() != 1) {
    response.setCodeServerError("Failed to fetch keys");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response rKeys = pb_response.payload(0);
  if (!rKeys.has_query()) {
    response.setCodeServerError("Key not found: " + path);
    return;
  }

  json::array node;
  for (const PB::Settings::Node &s : rKeys.query().nodes()) {
    json::object rs;
    rs["path"] = s.path();
    rs["key"] = s.key();
    rs["value"] = s.value();
    node.push_back(rs);
  }

  response.append(json::serialize(node));
}

void settings_controller::get_desc(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("settings.get", request, response)) return;

  if (!validate_arguments(1, what, response)) {
    return;
  }

  std::string path = what.str(1);

  PB::Settings::SettingsRequestMessage rm_fetch_paths;
  PB::Settings::SettingsRequestMessage::Request *payload_1 = rm_fetch_paths.add_payload();
  payload_1->mutable_inventory()->mutable_node()->set_path(path);
  payload_1->mutable_inventory()->set_recursive_fetch(request.get_bool("recursive", true));
  payload_1->mutable_inventory()->set_fetch_paths(true);
  payload_1->mutable_inventory()->set_fetch_keys(true);
  payload_1->mutable_inventory()->set_fetch_samples(request.get_bool("samples", false));
  payload_1->set_plugin_id(plugin_id);

  std::string str_response_1;
  core->settings_query(rm_fetch_paths.SerializeAsString(), str_response_1);
  PB::Settings::SettingsResponseMessage pb_response_1;
  pb_response_1.ParseFromString(str_response_1);

  if (pb_response_1.payload_size() != 1) {
    response.setCodeServerError("Failed to fetch keys");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response rKeys_1 = pb_response_1.payload(0);
  if (rKeys_1.inventory_size() == 0) {
    response.setCodeNotFound("Key not found: " + path);
    return;
  }
  typedef std::map<std::string, std::string> values_type;
  values_type values;

  if (true) {
    PB::Settings::SettingsRequestMessage rm_fetch_keys;
    PB::Settings::SettingsRequestMessage::Request *payload_2 = rm_fetch_keys.add_payload();
    payload_2->mutable_query()->mutable_node()->set_path(path);
    payload_2->mutable_query()->set_recursive(true);
    payload_2->mutable_query()->set_include_keys(true);
    payload_2->set_plugin_id(plugin_id);

    std::string str_response_2;
    core->settings_query(rm_fetch_keys.SerializeAsString(), str_response_2);
    PB::Settings::SettingsResponseMessage pb_response_2;
    pb_response_2.ParseFromString(str_response_2);

    if (pb_response_2.payload_size() != 1) {
      response.setCodeServerError("Failed to fetch keys");
      return;
    }

    const PB::Settings::SettingsResponseMessage::Response rKeys_2 = pb_response_2.payload(0);
    if (!rKeys_2.has_query()) {
      response.setCodeNotFound("Key not found: " + path);
      return;
    }

    for (const PB::Settings::Node &s : rKeys_2.query().nodes()) {
      if (!s.value().empty()) {
        values[s.path() + "$$$" + s.key()] = s.value();
      }
    }
  }

  json::array node;
  for (const PB::Settings::SettingsResponseMessage::Response::Inventory &s : rKeys_1.inventory()) {
    json::object rs;
    rs["path"] = s.node().path();
    rs["key"] = s.node().key();
    if (values.size() > 0) {
      values_type::const_iterator cit = values.find(s.node().path() + "$$$" + s.node().key());
      if (cit != values.end()) {
        rs["value"] = cit->second;
      } else {
        rs["value"] = s.info().default_value();
      }
    }
    rs["title"] = s.info().title();
    rs["icon"] = s.info().icon();
    rs["description"] = s.info().description();
    rs["is_advanced_key"] = s.info().advanced();
    rs["is_sample_key"] = s.info().sample();
    rs["is_template_key"] = s.info().is_template();
    rs["is_object"] = s.info().subkey();
    rs["sample_usage"] = s.info().sample_usage();
    rs["default_value"] = s.info().default_value();

    json::array plugins;
    for (const ::std::string &p : s.info().plugin()) {
      plugins.push_back(json::value(p));
    }
    rs["plugins"] = plugins;

    node.push_back(rs);
  }

  response.append(json::serialize(node));
}

void settings_controller::put(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("settings.put", request, response)) return;
  std::string response_pb;
  if (!core->settings_query(request.getData(), response_pb)) {
    response.setCodeServerError("500 Query failed");
    return;
  }
  response.append(response_pb);

  if (!validate_arguments(1, what, response)) {
    return;
  }
  std::string path = what.str(1);

  try {
    auto root = json::parse(request.getData());
    std::string object_type;

    PB::Settings::SettingsRequestMessage srm;

    int keys = 0;
    if (root.is_array()) {
      json::array a = root.as_array();
      for (const auto &v : a) {
        json::object o = v.as_object();
        auto current_path = o["path"].as_string();
        if (current_path.empty()) {
          current_path = path;
        }
        auto key = o["key"].as_string();
        if (key.empty()) {
          response.setCodeBadRequest("Key is required");
          return;
        }

        auto value = o["value"].as_string();

        PB::Settings::SettingsRequestMessage::Request *payload = srm.add_payload();
        payload->mutable_update()->mutable_node()->set_path(current_path.c_str());
        payload->mutable_update()->mutable_node()->set_key(key.c_str());
        payload->mutable_update()->mutable_node()->set_value(value.c_str());
        keys++;
      }
    } else {
      json::object o = root.as_object();
      auto current_path = o["path"].as_string();
      if (current_path.empty()) {
        current_path = path;
      }
      auto key = o["key"].as_string();
      if (key.empty()) {
        response.setCodeBadRequest("Key is required");
        return;
      }

      auto value = o["value"].as_string();

      PB::Settings::SettingsRequestMessage::Request *payload = srm.add_payload();
      payload->mutable_update()->mutable_node()->set_path(current_path.c_str());
      payload->mutable_update()->mutable_node()->set_key(key.c_str());
      payload->mutable_update()->mutable_node()->set_value(value.c_str());
      keys++;
    }

    std::string str_response;
    core->settings_query(srm.SerializeAsString(), str_response);

    PB::Settings::SettingsResponseMessage pb_response;
    pb_response.ParseFromString(str_response);

    json::object node;
    // TODO: Parse status here
    node["status"] = "success";
    node["keys"] = keys;
    response.append(json::serialize(node));

  } catch (const std::exception &e) {
    response.setCodeBadRequest("Problems parsing JSON");
  }
}

void settings_controller::command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("settings.put", request, response)) return;
  std::string response_pb;
  if (!core->settings_query(request.getData(), response_pb)) {
    response.setCodeServerError("500 Query failed");
    return;
  }
  response.append(response_pb);

  try {
    auto root = json::parse(request.getData());
    json::object o = root.as_object();
    auto command = o["command"].as_string();

    if (command == "reload") {
      if (!session->is_loggedin("settings.put", request, response)) return;
      if (!core->reload("delayed,service")) {
        response.setCodeServerError("500 Query failed");
        return;
      }
    } else {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *payload = srm.add_payload();
      if (command == "load") {
        payload->mutable_control()->set_command(PB::Settings::Command::LOAD);
      } else if (command == "save") {
        payload->mutable_control()->set_command(PB::Settings::Command::SAVE);
      } else {
        response.setCodeNotFound("Unknown command: " + static_cast<std::string>(command.data()));
        return;
      }
      std::string str_response;
      core->settings_query(srm.SerializeAsString(), str_response);

      PB::Settings::SettingsResponseMessage pb_response;
      pb_response.ParseFromString(str_response);
      // TODO: Parse status here
    }
    json::object node;
    node["status"] = "success";
    response.append(json::serialize(node));
  } catch (const std::exception &e) {
    response.setCodeBadRequest("Problems parsing JSON");
  }
}

void settings_controller::status(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("settings.get", request, response)) return;
  PB::Settings::SettingsRequestMessage rm;
  PB::Settings::SettingsRequestMessage::Request *payload = rm.add_payload();
  payload->mutable_status();
  payload->set_plugin_id(plugin_id);

  std::string str_response, json_response;
  core->settings_query(rm.SerializeAsString(), str_response);

  PB::Settings::SettingsResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  if (pb_response.payload_size() != 1) {
    response.setCodeNotFound("Failed to get status");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response &response_payload = pb_response.payload(0);
  if (!response_payload.has_status()) {
    response.setCodeNotFound("Failed to get status");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response::Status &status = response_payload.status();

  json::object node;
  node["context"] = status.context();
  node["type"] = status.type();
  node["has_changed"] = status.has_changed();
  response.append(json::serialize(node));
}
