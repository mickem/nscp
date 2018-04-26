#include "modules_controller.hpp"

#include <nscapi/nscapi_protobuf.hpp>

#include <str/xtos.hpp>
#include <file_helpers.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem/path.hpp>

#include <fstream>
#include <iostream>

#ifdef WIN32
#pragma warning(disable:4456)
#endif



modules_controller::modules_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController(version==1?"/api/v1/modules":"/api/v2/modules")
  , version(version)
{
	addRoute("GET", "/?$", this, &modules_controller::get_modules);
	addRoute("POST", "/([^/]+)/?$", this, &modules_controller::post_module);
	addRoute("GET", "/([^/]+)/?$", this, &modules_controller::get_module);
	addRoute("PUT", "/([^/]*)/?$", this, &modules_controller::put_module);
	addRoute("GET", "/([^/]+)/commands/([^/]*)/?$", this, &modules_controller::module_command);
}

void modules_controller::get_modules(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("modules.list", request, response))
    return;

  std::string fetch_all = request.get("all", "false");
  Plugin::RegistryRequestMessage rrm;
  Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_fetch_all(fetch_all == "true");
  payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_MODULE);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  Plugin::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json_spirit::Array root;

  BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response r, pb_response.payload()) {
	  BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response::Inventory i, r.inventory()) {
		  json_spirit::Object node;
		  node["name"] = i.name();
		  node["id"] = i.id();
		  node["title"] = i.info().title();
		  node["loaded"] = false;
		  node["enabled"] = false;
		  node["module_url"] = request.get_host() + "/api/v1/modules/" + i.name() + "/";
		  json_spirit::Object keys;
		  BOOST_FOREACH(const ::Plugin::Common::KeyValue &kvp, i.info().metadata()) {
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
  response.append(json_spirit::write(root));
}

void modules_controller::get_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("modules.get", request, response))
		return;

	if (what.size() != 2) {
		response.setCode(HTTP_NOT_FOUND);
		response.append("Module not found");
	}
	std::string module = what.str(1);

	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	payload->mutable_inventory()->set_name(module);
	payload->mutable_inventory()->set_fetch_all(false);
	payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_MODULE);
	std::string str_response;
	core->registry_query(rrm.SerializeAsString(), str_response);

	Plugin::RegistryResponseMessage pb_response;
	pb_response.ParseFromString(str_response);
	json_spirit::Object node;

	BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response r, pb_response.payload()) {
		if (r.inventory_size() == 0) {
			response.setCode(HTTP_NOT_FOUND);
			response.append("Module not found: " + module);
			return;
		}
		BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response::Inventory i, r.inventory()) {
			node["name"] = i.name();
			node["id"] = i.id();
			node["title"] = i.info().title();
			node["loaded"] = false;
			node["enabled"] = false;
			json_spirit::Object keys;
			BOOST_FOREACH(const ::Plugin::Common::KeyValue &kvp, i.info().metadata()) {
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
	response.append(json_spirit::write(node));
}

void modules_controller::module_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("modules", request, response))
		return;

	if (!validate_arguments(2, what, response)) {
		return;
	}
	std::string module = what.str(1);
	std::string command = what.str(2);

	if (command == "load") {
		if (!session->can("modules.load", request, response))
			return;
		load_module(module, response);
	} else if (command == "unload") {
		if (!session->can("modules.unload", request, response))
			return;
		unload_module(module, response);
	} else if (command == "enable") {
		if (!session->can("modules.enable", request, response))
			return;
		enable_module(module, response);
	} else if (command == "disable") {
		if (!session->can("modules.disable", request, response))
			return;
		disable_module(module, response);
	} else {
		response.setCode(HTTP_NOT_FOUND);
		response.append("unknown command: " + command);
	}
}



void modules_controller::load_module(std::string module, Mongoose::StreamResponse &http_response) {
	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_LOAD);
	payload->mutable_control()->set_name(module);
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(pb_response);

	if (version == 2) {
		helpers::parse_result_v2(response.payload(), http_response, "load " + module);
	} else {
		helpers::parse_result(response.payload(), http_response, "load " + module);
	}
}

void modules_controller::unload_module(std::string module, Mongoose::StreamResponse &http_response) {
	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_UNLOAD);
	payload->mutable_control()->set_name(module);
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(pb_response);

	if (version == 2) {
		helpers::parse_result_v2(response.payload(), http_response, "unload " + module);
	} else {
		helpers::parse_result(response.payload(), http_response, "unload " + module);
	}
}


void modules_controller::enable_module(std::string module, Mongoose::StreamResponse &http_response) {
	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_ENABLE);
	payload->mutable_control()->set_name(module);
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(pb_response);

	if (version == 2) {
		helpers::parse_result_v2(response.payload(), http_response, "enable " + module);
	} else {
		helpers::parse_result(response.payload(), http_response, "enable " + module);
	}
}

void modules_controller::disable_module(std::string module, Mongoose::StreamResponse &http_response) {
	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_DISABLE);
	payload->mutable_control()->set_name(module);
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	Plugin::RegistryResponseMessage response;
	response.ParseFromString(pb_response);

	if (version == 2) {
		helpers::parse_result_v2(response.payload(), http_response, "disable " + module);
	} else {
		helpers::parse_result(response.payload(), http_response, "disable " + module);
	}
}

void modules_controller::put_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("modules.put", request, response))
		return;

	if (!validate_arguments(1, what, response)) {
		return;
	}
	std::string module = what.str(1);

	try {

		json_spirit::Value root;
		std::string data = request.getData();
		json_spirit::read_or_throw(data, root);
		std::string object_type;
		json_spirit::Object o = root.getObject();
		bool target_is_loaded = o["loaded"].getBool();

		Plugin::RegistryRequestMessage rrm;
		Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
		payload->mutable_inventory()->set_name(module);
		payload->mutable_inventory()->set_fetch_all(false);
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_MODULE);
		std::string str_response;
		core->registry_query(rrm.SerializeAsString(), str_response);

		Plugin::RegistryResponseMessage pb_response;
		pb_response.ParseFromString(str_response);
		json_spirit::Object node;

		BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response r, pb_response.payload()) {
			BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response::Inventory i, r.inventory()) {
				if (i.name() == module) {
					bool is_loaded = false;
					BOOST_FOREACH(const ::Plugin::Common::KeyValue &kvp, i.info().metadata()) {
						if (kvp.key() == "loaded") {
							std::string v = kvp.value();
							is_loaded = v == "true";
						}
					}
					if (target_is_loaded != is_loaded) {
						if (target_is_loaded) {
							if (!session->can("modules.load", request, response))
								return;
							load_module(module, response);
							return;
						} else {
							if (!session->can("modules.unload", request, response))
								return;
							unload_module(module, response);
							return;
						}
					} else {
						response.setCode(HTTP_OK);
						response.append("No change");
						return;
					}
				}
			}
		}
	} catch (const json_spirit::ParseError &e) {
		response.setCode(HTTP_BAD_REQUEST);
		response.append("Problems parsing JSON");
	}
}



void modules_controller::post_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("modules.post", request, response))
		return;

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
		} catch (const json_spirit::ParseError &e) {
			response.setCode(HTTP_BAD_REQUEST);
			response.append("Failed to upload module");
		}

		Plugin::RegistryRequestMessage rrm;
		Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();

		payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
		payload->mutable_control()->set_command(Plugin::Registry_Command_LOAD);
		payload->mutable_control()->set_name(module);

		std::string str_response;
		core->registry_query(rrm.SerializeAsString(), str_response);

		Plugin::RegistryResponseMessage pb_response;
		pb_response.ParseFromString(str_response);
	} catch (const json_spirit::ParseError &e) {
		response.setCode(HTTP_BAD_REQUEST);
		response.append("Problems parsing JSON");
	}
}
