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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <client/simple_client.hpp>

#include <json_spirit.h>


namespace sh = nscapi::settings_helper;

using namespace std;
using namespace Mongoose;

WEBServer::WEBServer() {
}
WEBServer::~WEBServer() {}



Sessions g_sessions("nscpsessid");
error_handler log_data;

bool is_loggedin(Mongoose::Request &request, Mongoose::StreamResponse &response, bool respond = true) {
	Mongoose::Session &session = g_sessions.get(request, response);
	if (session.get("auth") != "true") {
		if (respond) {
			response.setCode(HTTP_FORBIDDEN);
			response << "500 Please login first";
		}
		return false;
	}
	return true;
}

class cli_handler : public client::cli_handler {
	int plugin_id;
	nscapi::core_wrapper* core;
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
		if (!is_loggedin(request, response))
			return;
		std::string command = request.get("command", "help");

		client_.handle_command(command);
		response << "{\"status\" : \"ok\"}";
	}
	void registry_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response))
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
		if (!is_loggedin(request, response))
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
		if (!is_loggedin(request, response))
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
		if (!is_loggedin(request, response))
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
		if (!is_loggedin(request, response))
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
		if (!is_loggedin(request, response))
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
		if (!is_loggedin(request, response))
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


	void auth_login(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (password.empty() || password != request.get("password")) {
			response.setCode(302);
			response.setHeader("Location", "/index.html?error=Invalid+password");
		} else {
			Mongoose::Session &session = g_sessions.get(request, response);
			session.setValue("auth", "true");
			response.setCode(302);
			response.setHeader("Location", request.getCookie("original_url", "/index.html"));
		}
	}
	void auth_logout(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		Mongoose::Session &session = g_sessions.get(request, response);
		session.setValue("auth", "false");
		response.setCode(302);
		response.setHeader("Location", "/index.html?msg=Logged+out");
	}

	void redirect_index(Mongoose::Request&, Mongoose::StreamResponse &response) {
		response.setCode(302);
		response.setHeader("Location", "/index.html");
	}

	void log_status(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response))
			return;
		error_handler::status status = log_data.get_status();
		response << "{ \"status\" : { \"count\" : " << status.error_count << ", \"error\" : \"" << status.last_error << "\"} }";
	}
	void log_messages(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response))
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
	void log_reset(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response))
			return;
		log_data.reset();
		response << "{\"status\" : \"ok\"}";
	}
	void reload(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response))
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
		addRoute("GET", "/auth/login", BaseController, auth_login);
		addRoute("GET", "/auth/logout", BaseController, auth_logout);
		addRoute("POST", "/auth/login", BaseController, auth_login);
		addRoute("POST", "/auth/logout", BaseController, auth_logout);
		addRoute("GET", "/core/reload", BaseController, reload);
		addRoute("GET", "/core/isalive", BaseController, alive);
		addRoute("GET", "/console/exec", BaseController, console_exec);
		addRoute("GET", "/", BaseController, redirect_index);
	}
};

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
		if (!is_js && !is_html && !is_css && !is_font && !is_jpg && !is_gif && !is_png)
			return NULL;

		boost::filesystem::path file = base / request.getUrl();

		StreamResponse *sr = new StreamResponse();
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
			if (!is_loggedin(request, *sr, false))
				file = base / "login.html";
			sr->setCookie("original_url", request.getUrl());
			sr->setHeader("Content-Type", "text/html");
		}
		if (is_css || is_font || is_gif || is_png || is_jpg) {
			sr->setHeader("Cache-Control", "max-age=3600"); //1 hour (60*60)

		}
		std::ifstream in(file.string().c_str(), ios_base::in|ios_base::binary);
		char buf[BUF_SIZE];

		do {
			in.read(&buf[0], BUF_SIZE);
			sr->write(&buf[0], in.gcount());
		} while (in.gcount() > 0);

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
	const nscapi::core_wrapper* core;
	const std::string password;
public: 

	RESTController(std::string password, nscapi::core_wrapper* core) : password(password), core(core) {}


	void handle_query(std::string obj, Mongoose::Request &request, Mongoose::StreamResponse &response) {
		if (!is_loggedin(request, response))
			return;

		Plugin::QueryRequestMessage rm;
		nscapi::protobuf::functions::create_simple_header(rm.mutable_header());
		Plugin::QueryRequestMessage::Request *payload = rm.add_payload();


		payload->set_command(obj);
		if (request.hasVariable("help"))
			payload->add_arguments("help");

		std::string pb_response, json_response;
		core->query(rm.SerializeAsString(), pb_response);
		core->protobuf_to_json("QueryResponseMessage", pb_response, json_response);
		response << json_response;
	}


	Response *process(Request &request) {
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
		"PASSWORD", "Password used to authenticate against server")

		;

		settings.register_all();
		settings.notify();

	if (mode == NSCAPI::normalStart) {
		try {
// 			std::list<std::string> errors;
// 			socket_helpers::validate_certificate(certificate, errors);
// 			NSC_LOG_ERROR_LISTS(errors);
			
			std::string path = get_core()->expand_path("${web-path}");
			boost::filesystem::path cert = get_core()->expand_path(certificate);
			server.reset(new Mongoose::Server(port.c_str(), path.c_str()));
			server->setOption("ssl_certificate", cert.string());
			server->registerController(new StaticController(path));
			server->registerController(new BaseController(password, get_core(), get_id()));
			server->registerController(new RESTController(password, get_core()));
		
			server->setOption("extra_mime_types", ".css=text/css,.js=application/javascript");
			server->start();
		} catch (const std::string &e) {
			NSC_LOG_ERROR("Error: " + e);
			return false;
		} catch (...) {
			NSC_LOG_ERROR("Error: ");
			return false;
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


bool WEBServer::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	if (request.arguments_size() > 0 && request.arguments(0) == "install")
		return install_server(request, response);
	if (request.arguments_size() > 0 && request.arguments(0) == "password")
		return password(request, response);
	nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp web [install|password] --help");
	return true;
}

bool WEBServer::install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response) {

	namespace po = boost::program_options;
	namespace pf = nscapi::protobuf::functions;
	po::variables_map vm;
	po::options_description desc;
	std::string allowed_hosts, cert, key, arguments = "false", chipers, insecure;
	unsigned int length = 1024;
	const std::string path = "/settings/NRPE/server";

	pf::settings_query q(get_id());
	q.get("/settings/default", "allowed hosts", "127.0.0.1");
	q.get(path, "insecure", "false");
	q.get(path, "certificate", "${certificate-path}/certificate.pem");
	q.get(path, "certificate key", "${certificate-path}/certificate_key.pem");
	q.get(path, "allow arguments", false);
	q.get(path, "allow nasty characters", false);
	q.get(path, "allowed ciphers", "");


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
		else if (val.path == path && val.key && *val.key == "allowed ciphers")
			chipers = val.get_string();
		else if (val.path == path && val.key && *val.key == "insecure")
			insecure = val.get_string();
		else if (val.path == path && val.key && *val.key == "allow arguments" && val.get_bool())
			arguments = "true";
		else if (val.path == path && val.key && *val.key == "allow nasty characters" && val.get_bool())
			arguments = "safe";
	}
	if (chipers == "ADH")
		insecure = "true";
	if (chipers == "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH") 
		insecure = "false";

	desc.add_options()
		("help", "Show help.")

		("allowed-hosts,h", po::value<std::string>(&allowed_hosts)->default_value(allowed_hosts), 
		"Set which hosts are allowed to connect")

		("certificate", po::value<std::string>(&cert)->default_value(cert), 
		"Length of payload (has to be same as on the server)")

		("certificate-key", po::value<std::string>(&key)->default_value(key), 
		"Client certificate to use")

		("insecure", po::value<std::string>(&insecure)->default_value(insecure)->implicit_value("true"), 
		"Use \"old\" legacy NRPE.")

		("payload-length,l", po::value<unsigned int>(&length)->default_value(1024), 
		"Length of payload (has to be same as on both the server and client)")

		("arguments", po::value<std::string>(&arguments)->default_value(arguments)->implicit_value("safe"), 
		"Allow arguments. false=don't allow, safe=allow non escape chars, all=allow all arguments.")

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
		result << "Enabling NRPE via SSH from: " << allowed_hosts << std::endl;
		s.set("/settings/default", "allowed hosts", allowed_hosts);
		s.set("/modules", "NRPEServer", "enabled");
		s.set("/settings/NRPE/server", "ssl", "true");
		if (insecure == "true") {
			result << "WARNING: NRPE is currently insecure." << std::endl;
			s.set("/settings/NRPE/server", "insecure", "true");
			s.set("/settings/NRPE/server", "allowed ciphers", "ADH");
		} else {
			result << "NRPE is currently reasonably secure using " << cert << " and " << key << "." << std::endl;
			s.set("/settings/NRPE/server", "insecure", "false");
			s.set("/settings/NRPE/server", "allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			s.set("/settings/NRPE/server", "certificate", cert);
			s.set("/settings/NRPE/server", "certificate key", key);
		}
		if (arguments == "all" || arguments == "unsafe") {
			result << "UNSAFE Arguments are allowed." << std::endl;
			s.set(path, "allow arguments", "true");
			s.set(path, "allow nasty characters", "true");
		} else if (arguments == "safe" || arguments == "true") {
			result << "SAFE Arguments are allowed." << std::endl;
			s.set(path, "allow arguments", "true");
			s.set(path, "allow nasty characters", "false");
		} else {
			result << "Arguments are NOT allowed." << std::endl;
			s.set(path, "allow arguments", "false");
			s.set(path, "allow nasty characters", "false");
		}
		s.set(path, "payload length", strEx::s::xtos(length));
		if (length != 1024)
			result << "NRPE is using non standard payload length " << length << " please use same configuration in check_nrpe." << std::endl;
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

		("set,s", po::value<std::string>(&password), 
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
