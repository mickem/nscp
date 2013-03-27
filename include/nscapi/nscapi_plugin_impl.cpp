#include <nscapi/nscapi_plugin_impl.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#include <iostream>

#include <protobuf/plugin.pb.h>

#include <format.hpp>

extern nscapi::helper_singleton* plugin_singleton;

nscapi::core_wrapper* nscapi::impl::simple_plugin::get_core() const {
	return plugin_singleton->get_core();
}

std::string nscapi::impl::simple_plugin::get_base_path() const {
	return get_core()->expand_path("${base-path}");
}

// void nscapi::impl::simple_plugin::register_command(std::string command, std::string description, std::list<std::string> aliases) {
// 
// 	Plugin::RegistryRequestMessage request;
// 	nscapi::protobuf::functions::create_simple_header(request.mutable_header());
// 
// 	Plugin::RegistryRequestMessage::Request *payload = request.add_payload();
// 	Plugin::RegistryRequestMessage::Request::Registration *regitem = payload->mutable_registration();
// 	regitem->set_plugin_id(get_id());
// 	regitem->set_type(Plugin::Registry_ItemType_QUERY);
// 	regitem->set_name(command);
// 	regitem->mutable_info()->set_title(command);
// 	regitem->mutable_info()->set_description(description);
// 	BOOST_FOREACH(const std::string &alias, aliases) {
// 		regitem->add_alias(alias);
// 	}
// 	std::string response_string;
// 	nscapi::plugin_singleton->get_core()->registry_query(request.SerializeAsString(), response_string);
// 	Plugin::RegistryResponseMessage response;
// 	response.ParseFromString(response_string);
// 	for (int i=0;i<response.payload_size();i++) {
// 		if (response.payload(i).result().status() != Plugin::Common_Status_StatusType_STATUS_OK)
// 			nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + command + ": " + response.payload(i).result().message());
// 	}
// }
// void nscapi::impl::simple_plugin::settings_register_key(std::string path, std::string key, NSCAPI::settings_type type, std::string title, std::string description, std::string defaultValue, bool advanced) {
// 	Plugin::SettingsRequestMessage request;
// 	nscapi::protobuf::functions::create_simple_header(request.mutable_header());
// 	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
// 	payload->set_plugin_id(get_id());
// 	Plugin::SettingsRequestMessage::Request::Registration *regitem = payload->mutable_registration();
// 	regitem->mutable_node()->set_key(key);
// 	regitem->mutable_node()->set_path(path);
// 	regitem->mutable_info()->set_title(title);
// 	regitem->mutable_info()->set_description(description);
// 	regitem->mutable_info()->mutable_default_value()->set_type(Plugin::Common_DataType_STRING);
// 	regitem->mutable_info()->mutable_default_value()->set_string_data(defaultValue);
// 	regitem->mutable_info()->set_advanced(advanced);
// 	std::string response_string;
// 	nscapi::plugin_singleton->get_core()->settings_query(request.SerializeAsString(), response_string);
// 	Plugin::SettingsResponseMessage response;
// 	response.ParseFromString(response_string);
// 	for (int i=0;i<response.payload_size();i++) {
// 		if (response.payload(i).result().status() != Plugin::Common_Status_StatusType_STATUS_OK)
// 			nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + path + "." + key + ": " + response.payload(i).result().message());
// 	}
// }
// void nscapi::impl::simple_plugin::settings_register_path(std::string path, std::string title, std::string description, bool advanced) {
// 	Plugin::SettingsRequestMessage request;
// 	nscapi::protobuf::functions::create_simple_header(request.mutable_header());
// 	Plugin::SettingsRequestMessage::Request *payload = request.add_payload();
// 	payload->set_plugin_id(get_id());
// 	Plugin::SettingsRequestMessage::Request::Registration *regitem = payload->mutable_registration();
// 	regitem->mutable_node()->set_path(path);
// 	regitem->mutable_info()->set_title(title);
// 	regitem->mutable_info()->set_description(description);
// 	regitem->mutable_info()->set_advanced(advanced);
// 	std::string response_string;
// 	nscapi::plugin_singleton->get_core()->settings_query(request.SerializeAsString(), response_string);
// 	Plugin::SettingsResponseMessage response;
// 	response.ParseFromString(response_string);
// 	for (int i=0;i<response.payload_size();i++) {
// 		if (response.payload(i).result().status() != Plugin::Common_Status_StatusType_STATUS_OK)
// 			nscapi::plugin_singleton->get_core()->log(NSCAPI::log_level::error, __FILE__, __LINE__, "Failed to register " + path + ": " + response.payload(i).result().message());
// 	}
// }
