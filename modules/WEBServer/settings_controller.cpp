#include "settings_controller.hpp"

#include <nscapi/nscapi_protobuf_settings.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <fstream>
#include <iostream>

settings_controller::settings_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController(version==1?"/api/v1/settings":"/api/v2/settings")
{
	addRoute("GET", "/descriptions(.*)$", this, &settings_controller::get_desc);
    addRoute("POST", "/command$", this, &settings_controller::command);
	addRoute("GET", "/status$", this, &settings_controller::status);
	addRoute("GET", "(.*)$", this, &settings_controller::get);
    addRoute("PUT", "(.*)$", this, &settings_controller::put);
}


void settings_controller::get(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("settings.get", request, response))
		return;

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

	json_spirit::Array node;
	for(const PB::Settings::Node &s: rKeys.query().nodes()) {
		json_spirit::Object rs;
		rs["path"] = s.path();
		rs["key"] = s.key();
		rs["value"] = s.value();
		node.push_back(rs);
	}

	response.append(json_spirit::write(node));
}



void settings_controller::get_desc(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("settings.get", request, response))
		return;

	if (!validate_arguments(1, what, response)) {
		return;
	}

	std::string path = what.str(1);

	PB::Settings::SettingsRequestMessage rm_fetch_paths;
	PB::Settings::SettingsRequestMessage::Request *payload = rm_fetch_paths.add_payload();
	payload->mutable_inventory()->mutable_node()->set_path(path);
	payload->mutable_inventory()->set_recursive_fetch(request.get_bool("recursive", true));
	payload->mutable_inventory()->set_fetch_paths(true);
	payload->mutable_inventory()->set_fetch_keys(true);
	payload->mutable_inventory()->set_fetch_samples(request.get_bool("samples", false));
	payload->set_plugin_id(plugin_id);

	std::string str_response;
	core->settings_query(rm_fetch_paths.SerializeAsString(), str_response);
	PB::Settings::SettingsResponseMessage pb_response;
	pb_response.ParseFromString(str_response);

	if (pb_response.payload_size() != 1) {
		response.setCodeServerError("Failed to fetch keys");
		return;
	}

	const PB::Settings::SettingsResponseMessage::Response rKeys = pb_response.payload(0);
	if (rKeys.inventory_size() == 0) {
		response.setCodeNotFound("Key not found: " + path);
		return;
	}
	typedef std::map<std::string, std::string> values_type;
	values_type values;

	if (true) {

		PB::Settings::SettingsRequestMessage rm_fetch_keys;
		PB::Settings::SettingsRequestMessage::Request *payload = rm_fetch_keys.add_payload();
		payload->mutable_query()->mutable_node()->set_path(path);
		payload->mutable_query()->set_recursive(true);
		payload->mutable_query()->set_include_keys(true);
		payload->set_plugin_id(plugin_id);

		std::string str_response;
		core->settings_query(rm_fetch_keys.SerializeAsString(), str_response);
		PB::Settings::SettingsResponseMessage pb_response;
		pb_response.ParseFromString(str_response);

		if (pb_response.payload_size() != 1) {
			response.setCodeServerError("Failed to fetch keys");
			return;
		}

		const PB::Settings::SettingsResponseMessage::Response rKeys = pb_response.payload(0);
		if (!rKeys.has_query()) {
			response.setCodeNotFound("Key not found: " + path);
			return;
		}

		for(const PB::Settings::Node &s: rKeys.query().nodes()) {
			if (!s.value().empty()) {
				values[s.path() + "$$$" + s.key()] = s.value();
			}
		}
	}

	json_spirit::Array node;
	for(const PB::Settings::SettingsResponseMessage::Response::Inventory &s: rKeys.inventory()) {
		json_spirit::Object rs;
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

		json_spirit::Array plugins;
		for(const ::std::string &p: s.info().plugin()) {
			plugins.push_back(p);
		}
		rs["plugins"] = plugins;

		node.push_back(rs);
	}

	response.append(json_spirit::write(node));
}


void settings_controller::put(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
    if (!session->is_loggedin("settings.put", request, response))
        return;
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

        json_spirit::Value root;
        std::string data = request.getData();
        json_spirit::read_or_throw(data, root);
        std::string object_type;

        PB::Settings::SettingsRequestMessage srm;

        int keys = 0;
		if (root.isArray()) {
			json_spirit::Array a = root.getArray();
			for(const json_spirit::Value & v: a) {

				json_spirit::Object o = v.getObject();
				std::string current_path = o["path"].getString();
				if (current_path.empty()) {
					current_path = path;
				}
				std::string key = o["key"].getString();
				if (key.empty()) {
					response.setCodeBadRequest("Key is required");
					return;
				}

				std::string value = o["value"].getString();

				PB::Settings::SettingsRequestMessage::Request* payload = srm.add_payload();
				payload->mutable_update()->mutable_node()->set_path(current_path);
				payload->mutable_update()->mutable_node()->set_key(key);
				payload->mutable_update()->mutable_node()->set_value(value);
                keys++;
			}
		}
		else {
			json_spirit::Object o = root.getObject();
			std::string current_path = o["path"].getString();
			if (current_path.empty()) {
				current_path = path;
			}
			std::string key = o["key"].getString();
			if (key.empty()) {
				response.setCodeBadRequest("Key is required");
				return;
			}

			std::string value = o["value"].getString();

			PB::Settings::SettingsRequestMessage::Request* payload = srm.add_payload();
			payload->mutable_update()->mutable_node()->set_path(current_path);
			payload->mutable_update()->mutable_node()->set_key(key);
			payload->mutable_update()->mutable_node()->set_value(value);
            keys++;
		}


        std::string str_response;
        core->settings_query(srm.SerializeAsString(), str_response);

        PB::Settings::SettingsResponseMessage pb_response;
        pb_response.ParseFromString(str_response);

        json_spirit::Object node;
        // TODO: Parse status here
        node["status"] = "success";
        node["keys"] = keys;
        response.append(json_spirit::write(node));

    } catch (const json_spirit::ParseError &e) {
        response.setCodeBadRequest("Problems parsing JSON");
    }
}




void settings_controller::command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
    if (!session->is_loggedin("settings.put", request, response))
        return;
    std::string response_pb;
    if (!core->settings_query(request.getData(), response_pb)) {
        response.setCodeServerError("500 Query failed");
        return;
    }
    response.append(response_pb);

    try {

        json_spirit::Value root;
        std::string data = request.getData();
        json_spirit::read_or_throw(data, root);
        json_spirit::Object o = root.getObject();
        std::string command = o["command"].getString();

        if (command == "reload") {
            if (!session->is_loggedin("settings.put", request, response))
                return;
            core->reload("delayed,service");
        } else {
            PB::Settings::SettingsRequestMessage srm;
            PB::Settings::SettingsRequestMessage::Request* payload = srm.add_payload();
            if (command == "load") {
                payload->mutable_control()->set_command(PB::Settings::Command::LOAD);
            } else if (command == "save") {
                payload->mutable_control()->set_command(PB::Settings::Command::SAVE);
            } else {
                response.setCodeNotFound("Unknown command: " + command);
                return;

            }
            std::string str_response;
            core->settings_query(srm.SerializeAsString(), str_response);

            PB::Settings::SettingsResponseMessage pb_response;
            pb_response.ParseFromString(str_response);
            // TODO: Parse status here
        }
        json_spirit::Object node;
        node["status"] = "success";
        response.append(json_spirit::write(node));
    } catch (const json_spirit::ParseError &e) {
        response.setCodeBadRequest("Problems parsing JSON");
    }
}


void settings_controller::status(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
    if (!session->is_loggedin("settings.get", request, response))
        return;
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

    json_spirit::Object node;
    node["context"] = status.context();
    node["type"] = status.type();
    node["has_changed"] = status.has_changed();
    response.append(json_spirit::write(node));

}
