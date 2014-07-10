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

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <settings/client/settings_client.hpp>
//#include <socket/socket_settings_helper.hpp>


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

class BaseController : public Mongoose::WebController
{
	const std::string password;
	const nscapi::core_wrapper* core;
	const unsigned int plugin_id;
	std::string status;
	boost::shared_mutex mutex_;

public: 

	BaseController(std::string password, nscapi::core_wrapper* core, unsigned int plugin_id) : password(password), core(core), plugin_id(plugin_id), status("ok") {}

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
		else if (type == "plugin")
			payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_PLUGIN);
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
		if (password != request.get("password")) {
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

	void redirect_index(Mongoose::Request &request, Mongoose::StreamResponse &response) {
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
		std::string str_position = request.get("pos", "0");
		int pos = strEx::s::stox<int>(str_position);
		std::string data = log_data.get_errors(pos);
		boost::replace_all(data, "\"", "'");
		boost::replace_all(data, "\\", "\\\\");
		boost::replace_all(data, "\n", "\\n");
		response << "{\"log\" : { \"data\" : \"" << data << "\", \"pos\" : \"" << pos << "\"} }";
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
		addRoute("GET", "/registry/inventory", BaseController, registry_inventory);
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

	int port;
	std::string password;

	settings.alias().add_path_to_settings()
		("WEB SERVER SECTION", "Section for WEB (WEBServer.dll) (check_WEB) protocol options.")
		;
	settings.alias().add_key_to_settings()
		("port", sh::int_key(&port, 8080),
		"PORT NUMBER", "Port to use for WEB.")
		;

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("password", sh::string_key(&password),
		"PASSWORD", "Password used to authenticate against server")

		;

		settings.register_all();
		settings.notify();

	if (mode == NSCAPI::normalStart) {

		server.reset(new Mongoose::Server(port));
		server->registerController(new StaticController(get_core()->expand_path("${web-path}")));
		server->registerController(new BaseController(password, get_core(), get_id()));
		server->registerController(new RESTController(password, get_core()));
		
		server->setOption("extra_mime_types", ".inc=text/html,.css=text/css,.js=application/javascript");
		server->start();

	}
	return true;
}

bool WEBServer::unloadModule() {
	try {
		if (server) {
			server->stop();
			boost::thread::sleep(boost::get_system_time() + boost::posix_time::seconds(2));
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("unload");
		return false;
	}
	return true;
}


void error_handler::add_message(bool is_error, std::string message) {
	{
		boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
		if (!lock.owns_lock())
			return;
		log_entries.push_back(message);
		if (is_error) {
			error_count_++;
			last_error_ = message;
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
std::string error_handler::get_errors(int &position) {
	std::string ret;
	boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
	if (!lock.owns_lock())
		return "";
	int count = log_entries.size()-position;
	std::list<std::string>::reverse_iterator cit = log_entries.rbegin();
	std::list<std::string>::reverse_iterator end = log_entries.rend();

	for (;cit != end && count > 0;++cit, --count, ++position) {
		ret += *cit;
		ret += "\n";
	}
	return ret;
}

void WEBServer::handleLogMessage(const Plugin::LogEntry::Entry &message) {
	log_data.add_message(message.level() == Plugin::LogEntry_Entry_Level_LOG_CRITICAL || message.level() == Plugin::LogEntry_Entry_Level_LOG_ERROR, message.message());
}
