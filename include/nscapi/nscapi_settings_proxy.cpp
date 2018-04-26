/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf.hpp>

#include <boost/foreach.hpp>

template<class T>
void report_errors(const T &response, nscapi::core_wrapper* core, const std::string &action) {
	for (int i = 0; i < response.payload_size(); i++) {
		if (response.payload(i).result().code() != Plugin::Common_Result_StatusCodeType_STATUS_OK)
			core->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to " + action + ": " + response.payload(i).result().message());
	}
}
void nscapi::settings_proxy::register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->mutable_node()->set_path(path);
	regitem->mutable_info()->set_title(title);
	regitem->mutable_info()->set_description(description);
	regitem->mutable_info()->set_advanced(advanced);
	regitem->mutable_info()->set_sample(sample);
	regitem->mutable_info()->set_subkey(false);
	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	if (!response.ParseFromString(response_string)) {
		core_->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to de-serialize the payload for " + path);
	}
	report_errors(response, core_, "register" + path);
}
void nscapi::settings_proxy::register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->mutable_node()->set_path(path);
	regitem->mutable_info()->set_title(title);
	regitem->mutable_info()->set_description(description);
	regitem->mutable_info()->set_advanced(advanced);
	regitem->mutable_info()->set_sample(sample);
	regitem->mutable_info()->set_subkey(true);
	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	if (!response.ParseFromString(response_string)) {
		core_->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to de-serialize the payload for " + path);
	}
	report_errors(response, core_, "register" + path);
}
void nscapi::settings_proxy::register_key(std::string path, std::string key, std::string title, std::string description, std::string defValue, bool advanced, bool sample) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->mutable_node()->set_key(key);
	regitem->mutable_node()->set_path(path);
	regitem->mutable_info()->set_title(title);
	regitem->mutable_info()->set_description(description);
	regitem->mutable_info()->set_default_value(defValue);
	regitem->mutable_info()->set_advanced(advanced);
	regitem->mutable_info()->set_sample(sample);
	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "register" + path + "." + key);
}


void nscapi::settings_proxy::register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->mutable_node()->set_path(path);
	regitem->mutable_info()->set_icon(icon);
	regitem->mutable_info()->set_title(title);
	regitem->mutable_info()->set_description(description);
	regitem->mutable_info()->set_advanced(false);
	regitem->mutable_info()->set_sample(false);
	regitem->set_fields(fields);
	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "register::tpl" + path);
}


std::string nscapi::settings_proxy::get_string(std::string path, std::string key, std::string def) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Query *item = payload->mutable_query();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->set_recursive(false);
	item->set_default_value(def);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return def;
	}
	return response.payload(0).query().node().value();
}
void nscapi::settings_proxy::set_string(std::string path, std::string key, std::string value) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->mutable_node()->set_value(value);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "update " + path + "." + key);
}
int nscapi::settings_proxy::get_int(std::string path, std::string key, int def) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Query *item = payload->mutable_query();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->set_recursive(false);
	item->set_default_value(nscapi::settings::settings_value::from_int(def));

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return def;
	}
	return nscapi::settings::settings_value::to_int(response.payload(0).query().node().value());
}
void nscapi::settings_proxy::set_int(std::string path, std::string key, int value) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->mutable_node()->set_value(nscapi::settings::settings_value::from_int(value));

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "update " + path + "." + key);
}
bool nscapi::settings_proxy::get_bool(std::string path, std::string key, bool def) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Query *item = payload->mutable_query();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->set_recursive(false);
	item->set_default_value(nscapi::settings::settings_value::from_bool(def));

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return def;
	}
	return nscapi::settings::settings_value::to_bool(response.payload(0).query().node().value());
}
void nscapi::settings_proxy::set_bool(std::string path, std::string key, bool value) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->mutable_node()->set_value(nscapi::settings::settings_value::from_bool(value));

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "update " + path + "." + key);
}
nscapi::settings_proxy::string_list nscapi::settings_proxy::get_sections(std::string path) {
	nscapi::settings_proxy::string_list ret;
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Query *item = payload->mutable_query();
	item->mutable_node()->set_path(path);
	item->set_recursive(true);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);

	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return ret;
	}

	std::size_t pos = path.size();
	BOOST_FOREACH(const ::Plugin::Settings::Node &n, response.payload(0).query().nodes()) {
		std::string s = n.path();
		s = s.substr(pos);
		if (s.size() > 0 && s[0] == '/') {
			s = s.substr(1);
		}
		ret.push_back(s);
	}
	return ret;
}
nscapi::settings_proxy::string_list nscapi::settings_proxy::get_keys(std::string path) {
	nscapi::settings_proxy::string_list ret;
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Query *item = payload->mutable_query();
	item->mutable_node()->set_path(path);
	item->set_recursive(false);
	item->set_include_keys(true);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);

	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return ret;
	}

	BOOST_FOREACH(const ::Plugin::Settings::Node &n, response.payload(0).query().nodes()) {
		ret.push_back(n.key());
	}
	return ret;
}
std::string nscapi::settings_proxy::expand_path(std::string key) {
	return core_->expand_path(key);
}

void nscapi::settings_proxy::err(const char* file, int line, std::string message) {
	core_->log(NSCAPI::log_level::error, file, line, message);
}
void nscapi::settings_proxy::warn(const char* file, int line, std::string message) {
	core_->log(NSCAPI::log_level::warning, file, line, message);
}
void nscapi::settings_proxy::info(const char* file, int line, std::string message) {
	core_->log(NSCAPI::log_level::info, file, line, message);
}
void nscapi::settings_proxy::debug(const char* file, int line, std::string message) {
	core_->log(NSCAPI::log_level::debug, file, line, message);
}

void nscapi::settings_proxy::save(const std::string context) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Control *item = payload->mutable_control();
	item->set_command(Plugin::Settings_Command_SAVE);
	if (!context.empty()) {
		item->set_context(context);
	}

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "save " + context);
}

void nscapi::settings_proxy::remove_key(std::string path, std::string key) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "delete " + path + "." + key);
}

void nscapi::settings_proxy::remove_path(std::string path) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_path(path);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	report_errors(response, core_, "delete " + path);
}
