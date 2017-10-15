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
void nscapi::settings_proxy::register_key(std::string path, std::string key, int type, std::string title, std::string description, nscapi::settings::settings_value defValue, bool advanced, bool sample) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Registration *regitem = payload->mutable_registration();
	regitem->mutable_node()->set_key(key);
	regitem->mutable_node()->set_path(path);
	regitem->mutable_info()->set_title(title);
	regitem->mutable_info()->set_description(description);
	if (defValue.string_value)
		regitem->mutable_info()->mutable_default_value()->set_string_data(defValue.get_string());
	else if (defValue.int_value)
		regitem->mutable_info()->mutable_default_value()->set_int_data(defValue.get_int());
	else if (defValue.bool_value)
		regitem->mutable_info()->mutable_default_value()->set_bool_data(defValue.get_bool());
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
	item->set_type(Plugin::Common_DataType_STRING);
	item->set_recursive(false);
	item->mutable_default_value()->set_string_data(def);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return def;
	}
	return response.payload(0).query().value().string_data();
}
void nscapi::settings_proxy::set_string(std::string path, std::string key, std::string value) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->mutable_value()->set_string_data(value);

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
	item->set_type(Plugin::Common_DataType_INT);
	item->set_recursive(false);
	item->mutable_default_value()->set_int_data(def);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return def;
	}
	return response.payload(0).query().value().int_data();
}
void nscapi::settings_proxy::set_int(std::string path, std::string key, int value) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->mutable_value()->set_int_data(value);

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
	item->set_type(Plugin::Common_DataType_BOOL);
	item->set_recursive(false);
	item->mutable_default_value()->set_bool_data(def);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);
	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return def;
	}
	return response.payload(0).query().value().bool_data();
}
void nscapi::settings_proxy::set_bool(std::string path, std::string key, bool value) {
	Plugin::SettingsRequestMessage request;
	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
	payload->set_plugin_id(plugin_id_);
	Plugin::SettingsRequestMessage::Request::Update *item = payload->mutable_update();
	item->mutable_node()->set_key(key);
	item->mutable_node()->set_path(path);
	item->mutable_value()->set_bool_data(value);

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
	item->set_type(Plugin::Common_DataType_LIST);
	item->set_recursive(true);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);

	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return ret;
	}

	const ::Plugin::Common_AnyDataType value = response.payload(0).query().value();

	for (int i = 0; i < value.list_data_size(); ++i) {
		ret.push_back(value.list_data(i));
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
	item->set_type(Plugin::Common_DataType_LIST);
	item->set_recursive(false);

	std::string response_string;
	core_->settings_query(request.SerializeAsString(), response_string);
	Plugin::SettingsResponseMessage response;
	response.ParseFromString(response_string);

	if (response.payload_size() != 1 || !response.payload(0).has_query()) {
		return ret;
	}

	const ::Plugin::Common_AnyDataType value = response.payload(0).query().value();

	for (int i = 0; i < value.list_data_size(); ++i) {
		ret.push_back(value.list_data(i));
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
