#include "legacy_controller.hpp"

#include "error_handler_interface.hpp"
#include "web_cli_handler.hpp"

#include <client/simple_client.hpp>

#include <nscapi/nscapi_protobuf.hpp>

#include <str/xtos.hpp>

#include <json_spirit.h>

#include <boost/asio/ip/address.hpp>
#include <boost/thread/locks.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

legacy_controller::legacy_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id, boost::shared_ptr<client::cli_client> client)
	: session(session)
	, core(core)
	, plugin_id(plugin_id)
	, client(client)
	, status("ok") {
	addRoute("GET", "/registry/control/module/load", this, &legacy_controller::registry_control_module_load);
	addRoute("GET", "/registry/control/module/unload", this, &legacy_controller::registry_control_module_unload);
	addRoute("GET", "/registry/inventory", this, &legacy_controller::registry_inventory);
	addRoute("GET", "/registry/inventory/modules", this, &legacy_controller::registry_inventory_modules);
	addRoute("GET", "/settings/inventory", this, &legacy_controller::settings_inventory);
	addRoute("POST", "/settings/query.json", this, &legacy_controller::settings_query_json);
	addRoute("POST", "/query.pb", this, &legacy_controller::run_query_pb);
	addRoute("POST", "/settings/query.pb", this, &legacy_controller::settings_query_pb);
	addRoute("GET", "/settings/status", this, &legacy_controller::settings_status);
	addRoute("GET", "/log/status", this, &legacy_controller::log_status);
	addRoute("GET", "/log/reset", this, &legacy_controller::log_reset);
	addRoute("GET", "/log/messages", this, &legacy_controller::log_messages);
	addRoute("GET", "/auth/token", this, &legacy_controller::auth_token);
	addRoute("GET", "/auth/logout", this, &legacy_controller::auth_logout);
	addRoute("POST", "/auth/token", this, &legacy_controller::auth_token);
	addRoute("POST", "/auth/logout", this, &legacy_controller::auth_logout);
	addRoute("GET", "/core/reload", this, &legacy_controller::reload);
	addRoute("GET", "/core/isalive", this, &legacy_controller::alive);
	addRoute("GET", "/console/exec", this, &legacy_controller::console_exec);
	addRoute("GET", "/metrics", this, &legacy_controller::get_metrics);
	addRoute("GET", "/", this, &legacy_controller::redirect_index);
}

std::string legacy_controller::get_status() {
	boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
	if (!lock.owns_lock())
		return "unknown";
	return status;
}
bool legacy_controller::set_status(std::string status_) {
	boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
	if (!lock.owns_lock())
		return false;
	status = status_;
	return true;
}

void legacy_controller::console_exec(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	std::string command = request.get("command", "help");

	client->handle_command(command);
	response.append("{\"status\" : \"ok\"}");
}
void legacy_controller::registry_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;

	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	if (request.get("all", "true") == "true")
		payload->mutable_inventory()->set_fetch_all(true);
	std::string type = request.get("type", "query");

	if (type == "query")
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_QUERY);
	else if (type == "command")
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_COMMAND);
	else if (type == "module")
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_MODULE);
	else if (type == "query-alias")
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_QUERY_ALIAS);
	else if (type == "all")
		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_ALL);
	else {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("500 Invalid type. Possible types are: query, command, plugin, query-alias, all");
		return;
	}
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
	response.append(json_response);
}
void legacy_controller::registry_control_module_load(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;

	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	std::string name = request.get("name", "");

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_LOAD);
	payload->mutable_control()->set_name(name);
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
	response.append(json_response);
}
void legacy_controller::registry_control_module_unload(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;

	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	std::string name = request.get("name", "");

	payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
	payload->mutable_control()->set_command(Plugin::Registry_Command_UNLOAD);
	payload->mutable_control()->set_name(name);
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
	response.append(json_response);
}
void legacy_controller::registry_inventory_modules(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;

	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	if (request.get("all", "true") == "true")
		payload->mutable_inventory()->set_fetch_all(true);
	std::string type = request.get("type", "query");

	payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_MODULE);
	std::string pb_response, json_response;
	core->registry_query(rrm.SerializeAsString(), pb_response);
	core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
	response.append(json_response);
}

void legacy_controller::settings_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	Plugin::SettingsRequestMessage rm;
	Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
	if (request.get("paths", "false") == "true")
		payload->mutable_inventory()->set_fetch_paths(true);
	if (request.get("keys", "false") == "true")
		payload->mutable_inventory()->set_fetch_keys(true);
	if (request.get("recursive", "false") == "true")
		payload->mutable_inventory()->set_recursive_fetch(true);
	if (request.get("samples", "false") == "true")
		payload->mutable_inventory()->set_fetch_samples(true);
	if (request.get("templates", "false") == "true")
		payload->mutable_inventory()->set_fetch_templates(true);
	if (request.get("desc", "false") == "true")
		payload->mutable_inventory()->set_descriptions(true);
	std::string path = request.get("path", "");
	if (!path.empty())
		payload->mutable_inventory()->mutable_node()->set_path(path);
	std::string key = request.get("key", "");
	if (!key.empty())
		payload->mutable_inventory()->mutable_node()->set_key(key);
	std::string module = request.get("module", "");
	if (!module.empty())
		payload->mutable_inventory()->set_plugin(module);
	payload->set_plugin_id(plugin_id);

	std::string pb_response, json_response;
	core->settings_query(rm.SerializeAsString(), pb_response);
	core->protobuf_to_json("SettingsResponseMessage", pb_response, json_response);
	response.append(json_response);
}
void legacy_controller::settings_query_json(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	std::string request_pb, response_pb, response_json;
	if (!core->json_to_protobuf(request.getData(), request_pb)) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("500 INvapid request");
		return;
	}
	core->settings_query(request_pb, response_pb);
	core->protobuf_to_json("SettingsResponseMessage", response_pb, response_json);
	response.append(response_json);
}
void legacy_controller::settings_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	std::string response_pb;
	if (!core->settings_query(request.getData(), response_pb)) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("500 QUery failed");
		return;
	}
	response.append(response_pb);
}
void legacy_controller::run_query_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	std::string response_pb;
	if (!core->query(request.getData(), response_pb)) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("500 QUery failed");
		return;
	}
	response.append(response_pb);
}
void legacy_controller::run_exec_pb(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	std::string response_pb;
	if (!core->exec_command("*", request.getData(), response_pb))
		return;
	response.append(response_pb);
}
void legacy_controller::settings_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	Plugin::SettingsRequestMessage rm;
	Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
	payload->mutable_status();
	payload->set_plugin_id(plugin_id);

	std::string pb_response, json_response;
	core->settings_query(rm.SerializeAsString(), pb_response);
	core->protobuf_to_json("SettingsResponseMessage", pb_response, json_response);
	response.append(json_response);
}

void legacy_controller::auth_token(Mongoose::Request &request, Mongoose::StreamResponse &response) {

	if (!session->is_allowed(request.getRemoteIp())) {
		response.setCode(HTTP_FORBIDDEN);
		response.append("403 Your not allowed");
		return;
	}

	if (session->validate_user("admin", request.get("password"))) {
		std::string token = session->generate_token("admin");
		response.setHeader("__TOKEN", token);
		response.append("{ \"status\" : \"ok\", \"auth token\": \"" + token + "\" }");
	} else {
		response.setCode(HTTP_FORBIDDEN);
		response.append("403 Invalid password");
	}
}
void legacy_controller::auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	std::string token = request.get("token");
	session->revoke_token(token);
	response.setHeader("__TOKEN", "");
	response.append("{ \"status\" : \"ok\", \"auth token\": \"\" }");
}

void legacy_controller::redirect_index(Mongoose::Request&, Mongoose::StreamResponse &response) {
	response.setCode(302);
	response.setHeader("Location", "/index.html");
}

void legacy_controller::log_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	error_handler_interface::status status = session->get_log_data()->get_status();
	std::string tmp = status.last_error;
	boost::replace_all(tmp, "\\", "/");
	response.append("{ \"status\" : { \"count\" : " + str::xtos(status.error_count) + ", \"error\" : \"" + tmp + "\"} }");
}
void legacy_controller::log_messages(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	json_spirit::Object root, log;
	json_spirit::Array data;

	std::string str_position = request.get("pos", "0");
	std::size_t pos = str::stox<std::size_t>(str_position);
	std::size_t ipp = 100000;
	std::size_t count = 0;
	std::list<std::string> levels;
	BOOST_FOREACH(const error_handler_interface::log_entry &e, session->get_log_data()->get_messages(levels, pos, ipp, count)) {
		json_spirit::Object node;
		node.insert(json_spirit::Object::value_type("file", e.file));
		node.insert(json_spirit::Object::value_type("line", e.line));
		node.insert(json_spirit::Object::value_type("type", e.type));
		node.insert(json_spirit::Object::value_type("date", e.date));
		node.insert(json_spirit::Object::value_type("message", e.message));
		data.push_back(node);
	}
	log.insert(json_spirit::Object::value_type("data", data));
	log.insert(json_spirit::Object::value_type("pos", pos));
	root.insert(json_spirit::Object::value_type("log", log));
	response.append(json_spirit::write(root));
}
void legacy_controller::get_metrics(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	response.append(session->get_metrics());
}
void legacy_controller::log_reset(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	session->reset_log();
	response.append("{\"status\" : \"ok\"}");
}
void legacy_controller::reload(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("legacy", request, response))
		return;
	core->reload("delayed,service");
	set_status("reload");
	response.append("{\"status\" : \"reload\"}");
}
void legacy_controller::alive(Mongoose::Request &request, Mongoose::StreamResponse &response) {
	response.append("{\"status\" : \"" + get_status() + "\"}");
}