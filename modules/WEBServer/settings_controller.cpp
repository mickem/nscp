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

settings_controller::settings_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController(version==1?"/api/v1/settings":"/api/v2/settings")
{
	addRoute("GET", "/descriptions(.*)$", this, &settings_controller::get_desc);
	addRoute("GET", "(.*)$", this, &settings_controller::get);
}


void settings_controller::get(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("settings.get", request, response))
		return;

	if (!validate_arguments(1, what, response)) {
		return;
	}

	std::string path = what.str(1);

	Plugin::SettingsRequestMessage rm;
	Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
	payload->mutable_query()->mutable_node()->set_path(path);
	payload->mutable_query()->set_recursive(true);
	payload->mutable_query()->set_include_keys(true);
	payload->set_plugin_id(plugin_id);

	std::string str_response;
	core->settings_query(rm.SerializeAsString(), str_response);
	Plugin::SettingsResponseMessage pb_response;
	pb_response.ParseFromString(str_response);

	if (pb_response.payload_size() != 1) {
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

	json_spirit::Array node;
	BOOST_FOREACH(const Plugin::Settings::Node &s, rKeys.query().nodes()) {
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

	Plugin::SettingsRequestMessage rm;
	Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
	payload->mutable_inventory()->mutable_node()->set_path(path);
	payload->mutable_inventory()->set_recursive_fetch(request.get_bool("recursive", true));
	payload->mutable_inventory()->set_fetch_paths(true);
	payload->mutable_inventory()->set_fetch_keys(true);
	payload->mutable_inventory()->set_fetch_samples(request.get_bool("samples", false));
	payload->set_plugin_id(plugin_id);

	std::string str_response;
	core->settings_query(rm.SerializeAsString(), str_response);
	Plugin::SettingsResponseMessage pb_response;
	pb_response.ParseFromString(str_response);

	if (pb_response.payload_size() != 1) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("Failed to fetch keys");
		return;
	}

	const Plugin::SettingsResponseMessage::Response rKeys = pb_response.payload(0);
	if (rKeys.inventory_size() == 0) {
		response.setCode(HTTP_NOT_FOUND);
		response.append("Key not found: " + path);
		return;
	}
	//typedef boost::unordered_map<std::string, std::string> values_type;
	typedef std::map<std::string, std::string> values_type;
	values_type values;

	if (true) {

		Plugin::SettingsRequestMessage rm;
		Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
		payload->mutable_query()->mutable_node()->set_path(path);
		payload->mutable_query()->set_recursive(true);
		payload->mutable_query()->set_include_keys(true);
		payload->set_plugin_id(plugin_id);

		std::string str_response;
		core->settings_query(rm.SerializeAsString(), str_response);
		Plugin::SettingsResponseMessage pb_response;
		pb_response.ParseFromString(str_response);

		if (pb_response.payload_size() != 1) {
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

		BOOST_FOREACH(const Plugin::Settings::Node &s, rKeys.query().nodes()) {
			if (s.has_value()) {
				values[s.path() + "$$$" + s.key()] = s.value();
			}
		}
	}

	json_spirit::Array node;
	BOOST_FOREACH(const Plugin::SettingsResponseMessage::Response::Inventory &s, rKeys.inventory()) {
		json_spirit::Object rs;
		rs["path"] = s.node().path();
		rs["key"] = s.node().key();
		if (values.size() > 0) {
			values_type::const_iterator cit = values.find(s.node().path() + "$$$" + s.node().key());
			if (cit != values.end()) {
				rs["value"] = cit->second;
			} else if (s.info().has_default_value()) {
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
		BOOST_FOREACH(const ::std::string &p, s.info().plugin()) {
			plugins.push_back(p);
		}
		rs["plugins"] = plugins;

		node.push_back(rs);
	}

	response.append(json_spirit::write(node));
}



