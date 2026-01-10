#include "modules_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <file_helpers.hpp>
#include <fstream>
#include <nscapi/nscapi_protobuf_registry.hpp>
#include <str/xtos.hpp>

#ifdef WIN32
#pragma warning(disable : 4456)
#endif

namespace json = boost::json;

modules_controller::modules_controller(const int version, const boost::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                       unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/modules" : "/api/v2/modules"), session(session), core(core), plugin_id(plugin_id), version(version) {
  addRoute("GET", "/?$", this, &modules_controller::get_modules);
  addRoute("POST", "/([^/]+)/?$", this, &modules_controller::post_module);
  addRoute("GET", "/([^/]+)/?$", this, &modules_controller::get_module);
  addRoute("PUT", "/([^/]*)/?$", this, &modules_controller::put_module);
  addRoute("GET", "/([^/]+)/commands/([^/]*)/?$", this, &modules_controller::module_command);
}

void modules_controller::get_modules(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("modules.list", request, response)) return;

  std::string fetch_all = request.get("all", "false");
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_fetch_all(fetch_all == "true");
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::MODULE);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json::array root;

  for (const PB::Registry::RegistryResponseMessage::Response r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory i : r.inventory()) {
      json::object node;
      node["name"] = i.name();
      node["id"] = i.id();
      node["title"] = i.info().title();
      node["loaded"] = false;
      node["enabled"] = false;
      node["module_url"] = request.get_host() + "/api/v1/modules/" + i.name() + "/";
      json::object keys;
      for (const PB::Common::KeyValue &kvp : i.info().metadata()) {
        if (kvp.key() == "loaded") {
          node["loaded"] = kvp.value() == "true";
        } else if (kvp.key() == "enabled") {
          node["enabled"] = kvp.value() == "true";
        } else {
          keys[kvp.key()] = kvp.value();
        }
      }
      node["metadata"] = keys;
      node["description"] = i.info().description();
      node["load_url"] = get_base(request) + "/" + i.id() + "/commands/load";
      node["unload_url"] = get_base(request) + "/" + i.id() + "/commands/unload";
      root.push_back(node);
    }
  }
  response.append(json::serialize(root));
}

void modules_controller::get_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("modules.get", request, response)) return;

  if (what.size() != 2) {
    response.setCodeNotFound("Module not found");
  }
  std::string module = what.str(1);

  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_name(module);
  payload->mutable_inventory()->set_fetch_all(false);
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::MODULE);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json::object node;

  for (const PB::Registry::RegistryResponseMessage::Response r : pb_response.payload()) {
    if (r.inventory_size() == 0) {
      response.setCodeNotFound("Module not found: " + module);
      return;
    }
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory i : r.inventory()) {
      node["name"] = i.name();
      node["id"] = i.id();
      node["title"] = i.info().title();
      node["loaded"] = false;
      node["enabled"] = false;
      json::object keys;
      for (const PB::Common::KeyValue &kvp : i.info().metadata()) {
        if (kvp.key() == "loaded") {
          node["loaded"] = kvp.value() == "true";
        } else if (kvp.key() == "enabled") {
          node["enabled"] = kvp.value() == "true";
        } else {
          keys[kvp.key()] = kvp.value();
        }
      }
      node["metadata"] = keys;
      node["description"] = i.info().description();
      node["load_url"] = get_base(request) + "/" + i.id() + "/commands/load";
      node["unload_url"] = get_base(request) + "/" + i.id() + "/commands/unload";
      node["enable_url"] = get_base(request) + "/" + i.id() + "/commands/enable";
      node["disable_url"] = get_base(request) + "/" + i.id() + "/commands/disable";
    }
  }
  response.append(json::serialize(node));
}

void modules_controller::module_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("modules", request, response)) return;

  if (!validate_arguments(2, what, response)) {
    return;
  }
  std::string module = what.str(1);
  std::string command = what.str(2);

  if (command == "load") {
    if (!session->can("modules.load", response)) return;
    load_module(module, response);
  } else if (command == "unload") {
    if (!session->can("modules.unload", response)) return;
    unload_module(module, response);
  } else if (command == "enable") {
    if (!session->can("modules.enable", response)) return;
    enable_module(module, response);
  } else if (command == "disable") {
    if (!session->can("modules.disable", response)) return;
    disable_module(module, response);
  } else {
    response.setCodeNotFound("unknown command: " + command);
  }
}

void modules_controller::load_module(std::string module, Mongoose::StreamResponse &http_response) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();

  payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
  payload->mutable_control()->set_command(PB::Registry::Command::LOAD);
  payload->mutable_control()->set_name(module);
  std::string pb_response, json_response;
  core->registry_query(rrm.SerializeAsString(), pb_response);
  PB::Registry::RegistryResponseMessage response;
  response.ParseFromString(pb_response);

  if (version == 2) {
    helpers::parse_result_v2(response.payload(), http_response, "load " + module);
  } else {
    helpers::parse_result(response.payload(), http_response, "load " + module);
  }
}

void modules_controller::unload_module(std::string module, Mongoose::StreamResponse &http_response) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();

  payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
  payload->mutable_control()->set_command(PB::Registry::Command::UNLOAD);
  payload->mutable_control()->set_name(module);
  std::string pb_response, json_response;
  core->registry_query(rrm.SerializeAsString(), pb_response);
  PB::Registry::RegistryResponseMessage response;
  response.ParseFromString(pb_response);

  if (version == 2) {
    helpers::parse_result_v2(response.payload(), http_response, "unload " + module);
  } else {
    helpers::parse_result(response.payload(), http_response, "unload " + module);
  }
}

void modules_controller::enable_module(std::string module, Mongoose::StreamResponse &http_response) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();

  payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
  payload->mutable_control()->set_command(PB::Registry::Command::ENABLE);
  payload->mutable_control()->set_name(module);
  std::string pb_response, json_response;
  core->registry_query(rrm.SerializeAsString(), pb_response);
  PB::Registry::RegistryResponseMessage response;
  response.ParseFromString(pb_response);

  if (version == 2) {
    helpers::parse_result_v2(response.payload(), http_response, "enable " + module);
  } else {
    helpers::parse_result(response.payload(), http_response, "enable " + module);
  }
}

void modules_controller::disable_module(std::string module, Mongoose::StreamResponse &http_response) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();

  payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
  payload->mutable_control()->set_command(PB::Registry::Command::DISABLE);
  payload->mutable_control()->set_name(module);
  std::string pb_response, json_response;
  core->registry_query(rrm.SerializeAsString(), pb_response);
  PB::Registry::RegistryResponseMessage response;
  response.ParseFromString(pb_response);

  if (version == 2) {
    helpers::parse_result_v2(response.payload(), http_response, "disable " + module);
  } else {
    helpers::parse_result(response.payload(), http_response, "disable " + module);
  }
}

void modules_controller::put_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  // TODO: THis works strangely and should be rewritten.
  if (!session->is_logged_in("modules.put", request, response)) return;

  if (!validate_arguments(1, what, response)) {
    return;
  }
  std::string module = what.str(1);

  try {
    auto root = json::parse(request.getData());
    std::string object_type;
    json::object o = root.as_object();
    bool target_is_loaded = o["loaded"].as_bool();

    PB::Registry::RegistryRequestMessage rrm;
    PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
    payload->mutable_inventory()->set_name(module);
    payload->mutable_inventory()->set_fetch_all(false);
    payload->mutable_inventory()->add_type(PB::Registry::ItemType::MODULE);
    std::string str_response;
    core->registry_query(rrm.SerializeAsString(), str_response);

    PB::Registry::RegistryResponseMessage pb_response;
    pb_response.ParseFromString(str_response);
    json::object node;

    for (const PB::Registry::RegistryResponseMessage::Response r : pb_response.payload()) {
      for (const PB::Registry::RegistryResponseMessage::Response::Inventory i : r.inventory()) {
        if (i.name() == module) {
          bool is_loaded = false;
          for (const PB::Common::KeyValue &kvp : i.info().metadata()) {
            if (kvp.key() == "loaded") {
              std::string v = kvp.value();
              is_loaded = v == "true";
            }
          }
          if (target_is_loaded != is_loaded) {
            if (target_is_loaded) {
              if (!session->can("modules.load", response)) return;
              load_module(module, response);
              return;
            } else {
              if (!session->can("modules.unload", response)) return;
              unload_module(module, response);
              return;
            }
          } else {
            response.setCodeOk();
            response.append("No change");
            return;
          }
        }
      }
    }
  } catch (const std::exception &e) {
    response.setCodeBadRequest("Problems parsing JSON");
  }
}

void modules_controller::post_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("modules.post", request, response)) return;

  if (!validate_arguments(1, what, response)) {
    return;
  }
  std::string module = what.str(1);

  try {
    try {
      boost::filesystem::path name = module;
      boost::filesystem::path file = core->expand_path("${module-path}/" + file_helpers::meta::get_filename(name) + ".zip");
      std::ofstream ofs(file.string().c_str(), std::ios::binary);
      ofs << request.getData();
      ofs.close();
    } catch (const std::exception &e) {
      response.setCodeBadRequest("Failed to upload module");
    }

    PB::Registry::RegistryRequestMessage rrm;
    PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();

    payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
    payload->mutable_control()->set_command(PB::Registry::Command::LOAD);
    payload->mutable_control()->set_name(module);

    std::string str_response;
    core->registry_query(rrm.SerializeAsString(), str_response);

    PB::Registry::RegistryResponseMessage pb_response;
    pb_response.ParseFromString(str_response);
  } catch (const std::exception &e) {
    response.setCodeBadRequest("Problems parsing JSON");
  }
}
