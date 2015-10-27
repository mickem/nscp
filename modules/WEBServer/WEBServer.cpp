/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include <iostream>
#include <fstream>
#include "WEBServer.h"
#include <strEx.h>
#include <time.h>
#include <timer.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/unordered_set.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_common_options.hpp>

#include <client/simple_client.hpp>

#include <json_spirit.h>


namespace sh = nscapi::settings_helper;

using namespace std;
using namespace Mongoose;

WEBServer::WEBServer() {
}
WEBServer::~WEBServer() {}

error_handler log_data;
metrics_handler metrics_store;
static const char alphanum[] = "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz";
class token_store {
	typedef boost::unordered_set<std::string> token_set;


	token_set tokens;
	std::string seed_token(int len) {
		std::string ret;
		for(int i=0; i < len; i++)
			ret += alphanum[rand() %  (sizeof(alphanum)-1)];
		return ret;
	}

public:
	bool validate(const std::string &token) {
		return tokens.find(token) != tokens.end();
	}

	std::string generate() {
		std::string token = seed_token(32);
		tokens.emplace(token);
		return token;
	}

	void revoke(const std::string &token) {
		token_set::iterator it = tokens.find(token);
		if (it != tokens.end())
			tokens.erase(it);
	}

};


token_store tokens;

bool is_loggedin(Mongoose::Request &request, Mongoose::StreamResponse &response, std::string gpassword, bool respond = true) {
	std::string token = request.readHeader("TOKEN");
	if (token.empty())
		token = request.get("__TOKEN", "");
	bool auth = false;
	if (token.empty()) {
		std::string password = request.readHeader("password");
		if (password.empty())
			password = request.get("password", "");
		auth = !password.empty() && password == gpassword;
	} else {
		auth = tokens.validate(token);
	}
	if (!auth) {
		if (respond) {
			response.setCode(HTTP_FORBIDDEN);
			response << "403 Please login first";
		}
		return false;
	}
	return true;
}

class cli_handler : public client::cli_handler {
	nscapi::core_wrapper* core;
	int plugin_id;
public:
	cli_handler(nscapi::core_wrapper* core, int plugin_id) : core(core), plugin_id(plugin_id) {}


	void output_message(const std::string &msg) {
		error_handler::log_entry e;
		e.date = "";
		e.file = "";
		e.line = 0;
		e.message = msg;
		e.type = "out";
		log_data.add_message(false, e);
	}
	virtual void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (core->should_log(NSCAPI::log_level::debug)) {
			core->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}

	virtual void log_error(std::string module, std::string file, int line, std::string msg) const
	{
		if (core->should_log(NSCAPI::log_level::debug)) {
			core->log(NSCAPI::log_level::error, file, line, msg);
		}
	}

	virtual int get_plugin_id() const { return plugin_id; }
	virtual nscapi::core_wrapper* get_core() const { return core; }

};

class BaseController : public Mongoose::WebController
{
	const std::string password;
	const nscapi::core_wrapper* core;
	const unsigned int plugin_id;
	std::string status;
	boost::shared_mutex mutex_;
	client::cli_client client_;

public: 

	BaseController(std::string password, nscapi::core_wrapper* core, unsigned int plugin_id) 
		: password(password)
		, core(core)
		, plugin_id(plugin_id)
		, status("ok")
		, client_(client::cli_handler_ptr(new cli_handler(core, plugin_id))){}

	std::string get_status() {
		boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
		if (!lock.owns_lock()) 
			return "unknown";
		return status;
	}
	bool set_status(std::string status_) {
		boost::shared_lock<boost::shared_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
		if (!lock.owns_lock()) 
			return false;
		status = status_;
		return true;
	}



	void console_exec(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		std::string command = request.get("command", "help");

		client_.handle_command(command);
		response << "{\"status\" : \"ok\"}";
	}
	void registry_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;

		Plugin::RegistryRequestMessage rrm;
		nscapi::protobuf::functions::create_simple_header(rrm.mutable_header());
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
			response << "500 Invalid type. Possible types are: query, command, plugin, query-alias, all";
			return;
		}
		std::string pb_response, json_response;
		core->registry_query(rrm.SerializeAsString(), pb_response);
		core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
		response << json_response;
	}
	void registry_control_module_load(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;

		Plugin::RegistryRequestMessage rrm;
		nscapi::protobuf::functions::create_simple_header(rrm.mutable_header());
		Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
		std::string name = request.get("name", "");

		payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
		payload->mutable_control()->set_command(Plugin::Registry_Command_LOAD);
		payload->mutable_control()->set_name(name);
		std::string pb_response, json_response;
		core->registry_query(rrm.SerializeAsString(), pb_response);
		core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
		response << json_response;
	}
	void registry_control_module_unload(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;

		Plugin::RegistryRequestMessage rrm;
		nscapi::protobuf::functions::create_simple_header(rrm.mutable_header());
		Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
		std::string name = request.get("name", "");

		payload->mutable_control()->set_type(Plugin::Registry_ItemType_MODULE);
		payload->mutable_control()->set_command(Plugin::Registry_Command_UNLOAD);
		payload->mutable_control()->set_name(name);
		std::string pb_response, json_response;
		core->registry_query(rrm.SerializeAsString(), pb_response);
		core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
		response << json_response;
	}
	void registry_inventory_modules(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;

		Plugin::RegistryRequestMessage rrm;
		nscapi::protobuf::functions::create_simple_header(rrm.mutable_header());
		Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
		if (request.get("all", "true") == "true")
			payload->mutable_inventory()->set_fetch_all(true);
		std::string type = request.get("type", "query");

		payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_MODULE);
		std::string pb_response, json_response;
		core->registry_query(rrm.SerializeAsString(), pb_response);
		core->protobuf_to_json("RegistryResponseMessage", pb_response, json_response);
		response << json_response;
	}

	void settings_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		Plugin::SettingsRequestMessage rm;
		nscapi::protobuf::functions::create_simple_header(rm.mutable_header());
		Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
		if (request.get("paths", "false") == "true")
			payload->mutable_inventory()->set_fetch_paths(true);
		if (request.get("keys", "false") == "true")
			payload->mutable_inventory()->set_fetch_keys(true);
		if (request.get("recursive", "false") == "true")
			payload->mutable_inventory()->set_recursive_fetch(true);
		if (request.get("samples", "false") == "true")
			payload->mutable_inventory()->set_fetch_samples(true);
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
		response << json_response;
	}
	void settings_query(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		std::string request_pb, response_pb, response_json;
		if (!core->json_to_protobuf(request.getData(), request_pb)) {
			NSC_LOG_ERROR("Failed to convert reuqest");
			return;
		}
		core->settings_query(request_pb, response_pb);
		core->protobuf_to_json("SettingsResponseMessage", response_pb, response_json);
		response << response_json;
	}
	void settings_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		Plugin::SettingsRequestMessage rm;
		nscapi::protobuf::functions::create_simple_header(rm.mutable_header());
		Plugin::SettingsRequestMessage::Request *payload = rm.add_payload();
		payload->mutable_status();
		payload->set_plugin_id(plugin_id);

		std::string pb_response, json_response;
		core->settings_query(rm.SerializeAsString(), pb_response);
		core->protobuf_to_json("SettingsResponseMessage", pb_response, json_response);
		response << json_response;
	}
	

	void auth_token(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (password.empty() || password != request.get("password")) {
			response.setCode(HTTP_FORBIDDEN);
			response << "403 Invalid password";
		} else {
			std::string token = tokens.generate();
			response.setHeader("__TOKEN", token);
			response << "{ \"status\" : \"ok\", \"auth token\": \"" << token << "\" }";
		}
	}
	void auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		std::string token = request.get("token");
		tokens.revoke(token);
		response.setHeader("__TOKEN", "");
		response << "{ \"status\" : \"ok\", \"auth token\": \"\" }";
	}

	void redirect_index(Mongoose::Request&, Mongoose::StreamResponse &response) {
		response.setCode(302);
		response.setHeader("Location", "/index.html");
	}

	void log_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		error_handler::status status = log_data.get_status();
		std::string tmp = status.last_error;
		boost::replace_all(tmp, "\\", "/");
		response << "{ \"status\" : { \"count\" : " << status.error_count << ", \"error\" : \"" << tmp << "\"} }";
	}
	void log_messages(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		json_spirit::Object root, log;
		json_spirit::Array data;

		std::string str_position = request.get("pos", "0");
		int pos = strEx::s::stox<int>(str_position);
		BOOST_FOREACH(const error_handler::log_entry &e, log_data.get_errors(pos)) {
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
		response << json_spirit::write(root);
	}
	void get_metrics(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		response << metrics_store.get();
	}
	void log_reset(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		log_data.reset();
		response << "{\"status\" : \"ok\"}";
	}
	void reload(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;
		core->reload("delayed,service");
		set_status("reload");
		response << "{\"status\" : \"reload\"}";
	}
	void alive(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		response << "{\"status\" : \"" + get_status() + "\"}";
	}

	void setup()
	{
		addRoute("GET", "/registry/control/module/load", BaseController, registry_control_module_load);
		addRoute("GET", "/registry/control/module/unload", BaseController, registry_control_module_unload);
		addRoute("GET", "/registry/inventory", BaseController, registry_inventory);
		addRoute("GET", "/registry/inventory/modules", BaseController, registry_inventory_modules);
		addRoute("GET", "/settings/inventory", BaseController, settings_inventory);
		addRoute("POST", "/settings/query.json", BaseController, settings_query);
		addRoute("GET", "/settings/status", BaseController, settings_status);
		addRoute("GET", "/log/status", BaseController, log_status);
		addRoute("GET", "/log/reset", BaseController, log_reset);
		addRoute("GET", "/log/messages", BaseController, log_messages);
		addRoute("GET", "/auth/token", BaseController, auth_token);
		addRoute("GET", "/auth/logout", BaseController, auth_logout);
		addRoute("POST", "/auth/token", BaseController, auth_token);
		addRoute("POST", "/auth/logout", BaseController, auth_logout);
		addRoute("GET", "/core/reload", BaseController, reload);
		addRoute("GET", "/core/isalive", BaseController, alive);
		addRoute("GET", "/console/exec", BaseController, console_exec);
		addRoute("GET", "/metrics", BaseController, get_metrics);
		addRoute("GET", "/", BaseController, redirect_index);
	}
};


bool nonAsciiChar(const char c) {  
	return !( (c>='A' && c < 'Z') || (c>='a' && c < 'z') || (c>='0' && c < '9') || c == '_');
} 
void stripNonAscii(string &str) { 
	str.erase(std::remove_if(str.begin(),str.end(), nonAsciiChar), str.end());
}

#define BUF_SIZE 4096

class StaticController : public Mongoose::WebController {
	boost::filesystem::path base;
public:
	StaticController(std::string path) : base(path){}


	Response *process(Request &request) {
		bool is_js = boost::algorithm::ends_with(request.getUrl(), ".js");
		bool is_css = boost::algorithm::ends_with(request.getUrl(), ".css");
		bool is_html = boost::algorithm::ends_with(request.getUrl(), ".html");
		bool is_gif = boost::algorithm::ends_with(request.getUrl(), ".gif");
		bool is_png = boost::algorithm::ends_with(request.getUrl(), ".png");
		bool is_jpg = boost::algorithm::ends_with(request.getUrl(), ".jpg");
		bool is_font = boost::algorithm::ends_with(request.getUrl(), ".ttf")
			|| boost::algorithm::ends_with(request.getUrl(), ".svg")
			|| boost::algorithm::ends_with(request.getUrl(), ".woff");
		StreamResponse *sr = new StreamResponse();
		if (!is_js && !is_html && !is_css && !is_font && !is_jpg && !is_gif && !is_png)  {
			sr->setCode(404);
			*sr << "Not found: " << request.getUrl();
			return sr;
		}

		boost::filesystem::path file = base / request.getUrl();
		if(!boost::filesystem::is_regular_file(file)) {
			NSC_LOG_ERROR("Failed to find: " + file.string());
			sr->setCode(404);
			*sr << "Not found: " << request.getUrl();
			return sr;
		}

		if (is_js)
			sr->setHeader("Content-Type", "application/javascript");
		else if (is_css)
			sr->setHeader("Content-Type", "text/css");
		else if (is_font)
			sr->setHeader("Content-Type", "text/html");
		else if (is_gif)
			sr->setHeader("Content-Type", "image/gif");
		else if (is_jpg)
			sr->setHeader("Content-Type", "image/jpeg");
		else if (is_png)
			sr->setHeader("Content-Type", "image/png");
		else {
			sr->setHeader("Content-Type", "text/html");
		}
		if (is_css || is_font || is_gif || is_png || is_jpg || is_js) {
			sr->setHeader("Cache-Control", "max-age=3600"); //1 hour (60*60)

		}
		std::ifstream in(file.string().c_str(), ios_base::in|ios_base::binary);
		char buf[BUF_SIZE];

		std::string token = request.get("__TOKEN");
		if (!tokens.validate(token))
			token = "";
		if (is_html) {
			std::string line;
			while (std::getline(in, line)) {
				if (line.empty())
					continue;
				std::string::size_type pos = line.find("<%=");
				if (pos != std::string::npos) {
					std::string::size_type end = line.find("%>", pos);
					if (end != std::string::npos) {
						pos+=3;
						std::string key = line.substr(pos, end-pos);
						if (boost::starts_with(key, "INCLUDE:")) {
							std::string fname = key.substr(8);
							stripNonAscii(fname);
							fname += ".html";
							boost::filesystem::path file2 = base / "include" / fname;
							NSC_DEBUG_MSG("File: " + file2.string());
							std::ifstream in2(file2.string().c_str(), ios_base::in|ios_base::binary);
							do {
								in2.read(&buf[0], BUF_SIZE);
								sr->write(&buf[0], in2.gcount());
							} while (in2.gcount() > 0);
							in2.close();
							line = line.substr(0, pos-3) + line.substr(end+2);
							boost::replace_all(line, "<%=TOKEN%>", token);
						} else {
							boost::replace_all(line, "<%=TOKEN%>", token);
							if (!token.empty())
								boost::replace_all(line, "<%=TOKEN_TAG%>", "?__TOKEN=" + token);
							else
								boost::replace_all(line, "<%=TOKEN_TAG%>", "");
						}
					}
				}
				sr->write(line.c_str(), line.size());
			}
		} else {
			do {
				in.read(&buf[0], BUF_SIZE);
				sr->write(&buf[0], in.gcount());
			} while (in.gcount() > 0);
		}
		in.close();
		return sr;
	}
	bool handles(string method, string url)
	{ 
		return boost::algorithm::ends_with(url, ".js")
			|| boost::algorithm::ends_with(url, ".css")
			|| boost::algorithm::ends_with(url, ".html")
			|| boost::algorithm::ends_with(url, ".ttf")
			|| boost::algorithm::ends_with(url, ".svg")
			|| boost::algorithm::ends_with(url, ".woff")
			|| boost::algorithm::ends_with(url, ".gif")
			|| boost::algorithm::ends_with(url, ".png")
			|| boost::algorithm::ends_with(url, ".jpg");
			;
	}

};


class RESTController : public Mongoose::WebController {
	const std::string password;
	const nscapi::core_wrapper* core;
public: 

	RESTController(std::string password, nscapi::core_wrapper* core) : password(password), core(core) {}


	void handle_query(std::string obj, Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response, password))
			return;

		Plugin::QueryRequestMessage rm;
		nscapi::protobuf::functions::create_simple_header(rm.mutable_header());
		Plugin::QueryRequestMessage::Request *payload = rm.add_payload();

		payload->set_command(obj);
		Request::arg_vector args = request.getVariablesVector();

		BOOST_FOREACH(const Request::arg_entry &e, args) {
			if (e.second.empty())
				payload->add_arguments(e.first);
			else
				payload->add_arguments(e.first + "=" + e.second);
		}

		std::string pb_response, json_response;
		core->query(rm.SerializeAsString(), pb_response);
		core->protobuf_to_json("QueryResponseMessage", pb_response, json_response);
		response << json_response;
	}


	Response *process(Request &request) {
		if (!handles(request.getMethod(), request.getUrl()))
			return NULL;
		StreamResponse *response = new StreamResponse();
		std::string url = request.getUrl();
		if (boost::algorithm::starts_with(url, "/query/")) {
			handle_query(url.substr(7), request, *response);
		} else {
			response->setCode(HTTP_SERVER_ERROR);
			(*response) << "Unknown REST node: " << url;
		}
		return response;
	}
	bool handles(string method, string url) {
		return boost::algorithm::starts_with(url, "/query/");
	}

};



bool WEBServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("WEB", alias, "server");

	std::string port;
	std::string password;
	std::string certificate;

	settings.alias().add_path_to_settings()
		("WEB SERVER SECTION", "Section for WEB (WEBServer.dll) (check_WEB) protocol options.")
		;
	settings.alias().add_key_to_settings()
		("port", sh::string_key(&port, "8443s"),
		"PORT NUMBER", "Port to use for WEB server.")
		;
	settings.alias().add_key_to_settings()
		("certificate", sh::string_key(&certificate, "${certificate-path}/certificate.pem"),
		"CERTIFICATE", "Ssl certificate to use for the ssl server")
		;

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("password", sh::string_key(&password),
		DEFAULT_PASSWORD_NAME, DEFAULT_PASSWORD_DESC)

		;

	settings.register_all();
	settings.notify();
	certificate = get_core()->expand_path(certificate);

	if (mode == NSCAPI::normalStart) {
		std::list<std::string> errors;
		socket_helpers::validate_certificate(certificate, errors);
		NSC_LOG_ERROR_LISTS(errors);
		std::string path = get_core()->expand_path("${web-path}");
		if(!boost::filesystem::is_regular_file(certificate) && port == "8443s")
			port = "8080";
			
		server.reset(new Mongoose::Server(port.c_str(), path.c_str()));
		if(!boost::filesystem::is_regular_file(certificate)) {
			NSC_LOG_ERROR("Certificate not found (disabling SSL): " + certificate);
		} else {
			NSC_DEBUG_MSG("Using certificate: " + certificate);
			server->setOption("ssl_certificate", certificate);
		}
		server->registerController(new BaseController(password, get_core(), get_id()));
		server->registerController(new RESTController(password, get_core()));
		server->registerController(new StaticController(path));
		
		server->setOption("extra_mime_types", ".css=text/css,.js=application/javascript");
		server->start();
		NSC_DEBUG_MSG("Loading webserver on port: " + port);
		if (password.empty()) {
			NSC_LOG_ERROR("No password set please run nscp web --help");
		}
	}
	return true;
}

bool WEBServer::unloadModule() {
	try {
		if (server) {
			server->stop();
			server.reset();
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("unload");
		return false;
	}
	return true;
}


void error_handler::add_message(bool is_error, const log_entry &message) {
	{
		boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!lock.owns_lock())
			return;
		log_entries.push_back(message);
		if (is_error) {
			error_count_++;
			last_error_ = message.message;
		}
	}
}
void error_handler::reset() {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return;
	log_entries.clear();
	last_error_ = "";
	error_count_ = 0;
}
error_handler::status error_handler::get_status() {
	status ret;
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return ret;
	ret.error_count = error_count_;
	ret.last_error = last_error_;
	return ret;
}
error_handler::log_list error_handler::get_errors(int &position) {
	log_list ret;
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return ret;
	log_list::iterator cit = log_entries.begin()+position;
	log_list::iterator end = log_entries.end();

	for (;cit != end;++cit) {
		ret.push_back(*cit);
		position++;
	}
	return ret;
}


void metrics_handler::set(const std::string &metrics) {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return;
	metrics_ = metrics;
}
std::string metrics_handler::get() {
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return "";
	return metrics_;
}

void WEBServer::handleLogMessage(const Plugin::LogEntry::Entry &message) {
	using namespace boost::posix_time;
	using namespace boost::gregorian;
	
	error_handler::log_entry entry;
	entry.line = message.line();
	entry.file = message.file();
	entry.message = message.message();
	entry.date = to_simple_string(second_clock::local_time());

	switch (message.level()) {
	case Plugin::LogEntry_Entry_Level_LOG_CRITICAL:
		entry.type = "critical";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_DEBUG:
		entry.type = "debug";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_ERROR:
		entry.type = "error";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_INFO:
		entry.type = "info";
		break;
	case Plugin::LogEntry_Entry_Level_LOG_WARNING:
		entry.type = "warning";
		break;
	default:
		entry.type = "unknown";

	}
	log_data.add_message(message.level() == Plugin::LogEntry_Entry_Level_LOG_CRITICAL || message.level() == Plugin::LogEntry_Entry_Level_LOG_ERROR, entry);
}


bool WEBServer::commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	const std::string &command = request.command();
	if (target_mode == NSCAPI::target_module && command.empty()) {
		nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp web [install|password] --help");
		return true;
	}
	if (command == "web") {
		if (request.arguments_size() > 0 && request.arguments(0) == "install")
			return install_server(request, response);
		if (request.arguments_size() > 0 && request.arguments(0) == "password")
			return password(request, response);
		nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp web [install|password] --help");
		return true;
	}
	return false;
}

bool WEBServer::install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {

	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string allowed_hosts, cert, key, port;
	const std::string path = "/settings/WEB/server";

	pf::settings_query q(get_id());
	q.get("/settings/default", "allowed hosts", "127.0.0.1");
	q.get(path, "certificate", "${certificate-path}/certificate.pem");
	q.get(path, "certificate key", "");
	q.get(path, "port", "8443s");


	get_core()->settings_query(q.request(), q.response());
	if (!q.validate_response()) {
		nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
		return true;
	}
	BOOST_FOREACH(const pf::settings_query::key_values &val, q.get_query_key_response()) {
		if (val.path == "/settings/default" && val.key && *val.key == "allowed hosts")
			allowed_hosts = val.get_string();
		else if (val.path == path && val.key && *val.key == "certificate")
			cert = val.get_string();
		else if (val.path == path && val.key && *val.key == "certificate key")
			key = val.get_string();
		else if (val.path == path && val.key && *val.key == "port")
			port = val.get_string();
	}

	desc.add_options()
		("help", "Show help.")

		("allowed-hosts,h", po::value<std::string>(&allowed_hosts)->default_value(allowed_hosts), 
		"Set which hosts are allowed to connect")

		("certificate", po::value<std::string>(&cert)->default_value(cert), 
		"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>(&key)->default_value(key), 
		"Client certificate to use")

		("port", po::value<std::string>(&port)->default_value(port), 
		"Port to use suffix with s for ssl")

		;

	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help")) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return true;
		}
		std::stringstream result;

		nscapi::protobuf::functions::settings_query s(get_id());
		result << "Enabling WEB from (currently not supported): " << allowed_hosts << std::endl;
		s.set("/settings/default", "allowed hosts", allowed_hosts);
		s.set("/modules", "WEBServer", "enabled");
		if (port.find('s') != std::string::npos) {
			result << "HTTP(s) is enabled using " << get_core()->expand_path(cert);
			if (!key.empty())
				result << " and " << get_core()->expand_path(key);
			result << "." << std::endl;
		}
		s.set(path, "certificate", cert);
		s.set(path, "certificate key", key);
		result << "Point your browser to: " << boost::replace_all_copy(port, "s", "") << std::endl;
		s.set(path, "port", port);
		s.save();
		get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return true;
		}
		nscapi::protobuf::functions::set_response_good(*response, result.str());
		return true;
	} catch (const std::exception &e) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
		return true;
	} catch (...) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
		return true;
	}
}


bool WEBServer::password(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {
	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;

	std::string password;
	bool display = false, setweb = false;

	desc.add_options()
		("help", "Show help.")

		("password,s", po::value<std::string>(&password), 
		"Set the new password")

		("display,d", po::bool_switch(&display), 
		"Display the current configured password")

		("only-web", po::bool_switch(&setweb), 
		"Set the password for WebServer only (if not specified the default password is used)")

		;
	try {
		nscapi::program_options::basic_command_line_parser cmd(request);
		cmd.options(desc);

		po::parsed_options parsed = cmd.run();
		po::store(parsed, vm);
		po::notify(vm);

		if (vm.count("help")) {
			nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
			return true;
		}
	} catch (const std::exception &e) {
		nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
		return true;
	}

	if (display) {
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("WEB", "", "server");

		settings.alias().add_parent("/settings/default").add_key_to_settings()

			("password", sh::string_key(&password),
			"PASSWORD", "Password used to authenticate against server")

			;

		settings.register_all();
		settings.notify();
		if (password.empty())
			nscapi::protobuf::functions::set_response_good(*response, "No password set you will not be able to login");
		else
			nscapi::protobuf::functions::set_response_good(*response, "Current password: " + password);
	} else if (!password.empty()) {
		nscapi::protobuf::functions::settings_query s(get_id());
		if (setweb) {
			s.set("/settings/default", "password", password);
			s.set("/settings/WEB/server", "password", "");
		} else {
			s.set("/settings/WEB/server", "password", password);
		}

		s.save();
		get_core()->settings_query(s.request(), s.response());
		if (!s.validate_response()) {
			nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
			return true;
		}
		nscapi::protobuf::functions::set_response_good(*response, "Password updated successfully, please restart nsclient++ for changes to affect.");

	} else {
		nscapi::protobuf::functions::set_response_bad(*response, nscapi::program_options::help(desc));
	}
	return true;
}



void build_metrics(json_spirit::Object &metrics, const Plugin::Common::MetricsBundle & b)
{
	json_spirit::Object node;
	BOOST_FOREACH(const Plugin::Common::MetricsBundle &b2, b.children()) {
		build_metrics(node, b2);
	}
	BOOST_FOREACH(const Plugin::Common::Metric &v, b.value()) {
		const ::Plugin::Common_AnyDataType &value = v.value();
		if (value.has_int_data())
			node.insert(json_spirit::Object::value_type(v.key(), v.value().int_data()));
		else if (value.has_string_data())
			node.insert(json_spirit::Object::value_type(v.key(), v.value().string_data()));
		else if (value.has_float_data())
			node.insert(json_spirit::Object::value_type(v.key(), v.value().float_data()));
		else
			node.insert(json_spirit::Object::value_type(v.key(), "TODO"));
	}
	metrics.insert(json_spirit::Object::value_type(b.key(), node));
}
void WEBServer::submitMetrics(const Plugin::MetricsMessage::Response &response) {
	json_spirit::Object metrics;
	BOOST_FOREACH(const Plugin::Common::MetricsBundle &b, response.bundles()) {
		build_metrics(metrics, b);
	}
	metrics_store.set(json_spirit::write(metrics));
}