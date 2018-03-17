#include "settings_controller.hpp"

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

settings_controller::settings_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController("/api/v1/settings")
{
	addRoute("GET", "(/)$", this, &settings_controller::get_section);
	addRoute("GET", "(/.+)/$", this, &settings_controller::get_section);
	addRoute("GET", "(/.+)/(.+)$", this, &settings_controller::get_key);
}


void settings_controller::get_section(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("settings.list", request, response))
		return;

	if (!validate_arguments(1, what, response)) {
		return;
	}

	std::string path = what.str(1);

	Plugin::SettingsRequestMessage rm;
	Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
	payload->mutable_query()->mutable_node()->set_path(path);
	payload->mutable_query()->set_recursive(false);
	payload->set_plugin_id(plugin_id);
	payload = rm.add_payload();
	payload->mutable_query()->mutable_node()->set_path(path);
	payload->mutable_query()->set_recursive(true);
	payload->set_plugin_id(plugin_id);

	std::string str_response;
	core->settings_query(rm.SerializeAsString(), str_response);
	Plugin::SettingsResponseMessage pb_response;
	pb_response.ParseFromString(str_response);
	json_spirit::Object node;
	node["path"] = path;

	if (pb_response.payload_size() != 2) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("Failed to fetch keys");
		return;
	}

	const Plugin::SettingsResponseMessage::Response rKeys = pb_response.payload(0);
	if (!rKeys.has_query()) {
		response.setCode(HTTP_NOT_FOUND);
		response.append("Key not found: " + path);
		return;
	}

	json_spirit::Array keys;
	BOOST_FOREACH(const std::string &s, rKeys.query().value().list_data()) {
		keys.push_back(s);
	}
	node["keys"] = keys;

	const Plugin::SettingsResponseMessage::Response rPath = pb_response.payload(1);
	if (!rPath.has_query()) {
		response.setCode(HTTP_NOT_FOUND);
		response.append("Key not found: " + path);
		return;
	}
	json_spirit::Array paths;
	BOOST_FOREACH(const std::string &s, rPath.query().value().list_data()) {
		paths.push_back(s);
	}
	node["paths"] = paths;


	response.append(json_spirit::write(node));
}

void settings_controller::get_key(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("settings", request, response))
		return;

	if (!validate_arguments(2, what, response)) {
		return;
	}
	std::string path = what.str(1);
	std::string key = what.str(2);

	if (!session->can("settings.get", request, response))
		return;

	Plugin::SettingsRequestMessage rm;
	Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
	payload->mutable_query()->mutable_node()->set_path(path);
	payload->mutable_query()->mutable_node()->set_key(key);
	payload->mutable_query()->set_recursive(false);
	payload->set_plugin_id(plugin_id);

	std::string str_response;
	core->settings_query(rm.SerializeAsString(), str_response);
	Plugin::SettingsResponseMessage pb_response;
	pb_response.ParseFromString(str_response);
	json_spirit::Object node;

	BOOST_FOREACH(const Plugin::SettingsResponseMessage::Response r, pb_response.payload()) {
		if (!r.has_query()) {
			response.setCode(HTTP_NOT_FOUND);
			response.append("Key not found: " + path + "/" + key);
			return;
		}
		const Plugin::SettingsResponseMessage::Response::Query &i = r.query();
		node["path"] = i.node().path();
		node["key"] = i.node().key();
		if (i.value().has_string_data()) {
			node["value"] = i.value().string_data();
		} else if (i.value().has_int_data()) {
			node["value"] = i.value().int_data();
		} else if (i.value().has_bool_data()) {
			node["value"] = i.value().bool_data();
		}
	}
	response.append(json_spirit::write(node));
}




