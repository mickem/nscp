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
#include "stdafx.h"
#include <iostream>
#include <fstream>
#include "WEBServer.h"
#include <strEx.h>
#include <time.h>
#include "handler_impl.hpp"

#include <boost/algorithm/string.hpp>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <settings/client/settings_client.hpp>
#include <socket/socket_settings_helper.hpp>

namespace sh = nscapi::settings_helper;


WEBServer::WEBServer() {
}
WEBServer::~WEBServer() {}


using namespace std;
using namespace Mongoose;


class BaseController : public Mongoose::WebController
{
	nscapi::core_wrapper* core;
public: 

	BaseController(nscapi::core_wrapper* core) : core(core) {}



	void registry_inventory(Mongoose::Request &request, Mongoose::StreamResponse &response) {

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


	void session(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		Mongoose::Session &session = getSession(request, response);

		if (session.hasValue("try")) {
			response << "Session value: " << session.get("try");
		} else {
			ostringstream val;
			val << time(NULL);
			session.setValue("try", val.str());
			response << "Session value set to: " << session.get("try");
		}
	}

	void redirect_index(Mongoose::Request &request, Mongoose::StreamResponse &response) {
		response.setCode(302);
		response.setHeader("Location", "/index.html");
		response.setCookie("original_url", request.getUrl());
	}

	void setup()
	{
		addRoute("GET", "/registry/inventory", BaseController, registry_inventory);
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
		if (!is_js && !is_html && !is_css)
			return NULL;

		NSC_DEBUG_MSG("---> Request: " + request.getUrl());

		boost::filesystem::path file = base / request.getUrl();

		NSC_DEBUG_MSG(request.getUrl() + " as " + file.string());


		std::ifstream in(file.string().c_str(), ios_base::in|ios_base::binary);

		StreamResponse *sr = new StreamResponse();
		if (is_js)
			sr->setHeader("Content-Type", "application/javascript");
		else if (is_css)
			sr->setHeader("Content-Type", "text/css");
		else
			sr->setHeader("Content-Type", "text/html");
		char buf[BUF_SIZE];

		do {
			in.read(&buf[0], BUF_SIZE);      // Read at most n bytes into
			sr->write(&buf[0], in.gcount()); // buf, then write the buf to
		} while (in.gcount() > 0);          // the output.

		// Check streams for problems...

		in.close();
		return sr;
	}
	bool handles(string method, string url)
	{ 
		return boost::algorithm::ends_with(url, ".js")
			|| boost::algorithm::ends_with(url, ".css")
			|| boost::algorithm::ends_with(url, ".html");
	}

};


class RESTController : public Mongoose::WebController {
	nscapi::core_wrapper* core;
public: 

	RESTController(nscapi::core_wrapper* core) : core(core) {}


	void handle_query(std::string obj, Mongoose::Request &request, Mongoose::StreamResponse &response) {


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

	settings.alias().add_path_to_settings()
		("WEB SERVER SECTION", "Section for WEB (WEBServer.dll) (check_WEB) protocol options.")
		;
/*
	settings.alias().add_key_to_settings()
		("port", sh::string_key(&info_.port_, "5666"),
		"PORT NUMBER", "Port to use for WEB.")

		("payload length", sh::int_fun_key<unsigned int>(boost::bind(&WEB::server::handler::set_payload_length, handler_, _1), 1024),
		"PAYLOAD LENGTH", "Length of payload to/from the WEB agent. This is a hard specific value so you have to \"configure\" (read recompile) your WEB agent to use the same value for it to work.", true)

		("allow arguments", sh::bool_fun_key<bool>(boost::bind(&WEB::server::handler::set_allow_arguments, handler_, _1), false),
		"COMMAND ARGUMENT PROCESSING", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

		("allow nasty characters", sh::bool_fun_key<bool>(boost::bind(&WEB::server::handler::set_allow_nasty_arguments, handler_, _1), false),
		"COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.")

		("performance data", sh::bool_fun_key<bool>(boost::bind(&WEB::server::handler::set_perf_data, handler_, _1), true),
		"PERFORMANCE DATA", "Send performance data back to nagios (set this to 0 to remove all performance data).", true)

		;
		*/
//	socket_helpers::settings_helper::add_core_server_opts(settings, info_);
//	socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true, "", "", "ADH");

/*
	settings.alias().add_parent("/settings/default").add_key_to_settings()
	
		("encoding", sh::string_key(&handler_->encoding_, ""),
		"WEB PAYLOAD ENCODING", "", true)
		;
		*/
		settings.register_all();
		settings.notify();

		/*
#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
		return false;
	}
#endif
	*/
	if (mode == NSCAPI::normalStart) {

		//MyController myController;
		server.reset(new Mongoose::Server(8080));
		server->registerController(new StaticController(get_core()->expand_path("${web-path}")));
		server->registerController(new BaseController(get_core()));
		server->registerController(new RESTController(get_core()));
		
//		server->setOption("enable_directory_listing", "true");
		server->setOption("extra_mime_types", ".inc=text/html,.css=text/css,.js=application/javascript");
		server->start();


		/*
		if (handler_->get_payload_length() != 1024)
			NSC_DEBUG_MSG_STD("Non-standard buffer length (hope you have recompiled check_WEB changing #define MAX_PACKETBUFFER_LENGTH = " + strEx::s::xtos(handler_->get_payload_length()));
		NSC_LOG_ERROR_LISTS(info_.validate());

		std::list<std::string> errors;
		info_.allowed_hosts.refresh(errors);
		NSC_LOG_ERROR_LISTS(errors);
		NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

		boost::asio::io_service io_service_;

		server_.reset(new WEB::server::server(info_, handler_));
		if (!server_) {
			NSC_LOG_ERROR_STD("Failed to create server instance!");
			return false;
		}
		server_->start();
		*/
	}
	return true;
}

bool WEBServer::unloadModule() {
	try {
		if (server)
		server->stop();
// 		if (server_) {
// 			server_->stop();
// 			server_.reset();
// 		}
	} catch (...) {
		NSC_LOG_ERROR_EX("unload");
		return false;
	}
	return true;
}
