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
#include "WEBServer.h"
#include <strEx.h>
#include <time.h>
#include "handler_impl.hpp"

#include <settings/client/settings_client.hpp>
#include <socket/socket_settings_helper.hpp>

namespace sh = nscapi::settings_helper;


WEBServer::WEBServer() {
}
WEBServer::~WEBServer() {}


using namespace std;
using namespace Mongoose;


class MyController : public Mongoose::WebController
{
public: 
	void hello(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		response << "Hello " << htmlEntities(request.get("name", "... what's your name ?")) << endl;
	}

	void form(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		response << "<form method=\"post\">" << endl;
		response << "<input type=\"text\" name=\"test\" /><br >" << endl;
		response << "<input type=\"submit\" value=\"Envoyer\" />" << endl;
		response << "</form>" << endl;
	}

	void formPost(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		response << "Test=" << htmlEntities(request.get("test", "(unknown)"));
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

	void forbid(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		response.setCode(HTTP_FORBIDDEN);
		response << "403 forbidden demo";
	}

	void exception(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		throw string("Exception example");
	}

	void uploadForm(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		response << "<h1>File upload demo (don't forget to create a tmp/ directory)</h1>";
		response << "<form enctype=\"multipart/form-data\" method=\"post\">";
		response << "Choose a file to upload: <input name=\"file\" type=\"file\" /><br />";
		response << "<input type=\"submit\" value=\"Upload File\" />";
		response << "</form>";
	}

	void upload(Mongoose::Request &request, Mongoose::StreamResponse &response)
	{
		request.handleUploads();

		// Iterate through all the uploaded files
		vector<Mongoose::UploadFile>::iterator it = request.uploadFiles.begin();
		for (; it != request.uploadFiles.end(); it++) {
			Mongoose::UploadFile file = *it;
			file.saveTo("tmp/");
			response << "Uploaded file: " << file.getName() << endl;
		}
	}

	void setup()
	{
		// Hello demo
		addRoute("GET", "/hello", MyController, hello);
		addRoute("GET", "/", MyController, hello);

		// Form demo
		addRoute("GET", "/form", MyController, form);
		addRoute("POST", "/form", MyController, formPost);

		// Session demo
		addRoute("GET", "/session", MyController, session);

		// Exception example
		addRoute("GET", "/exception", MyController, exception);

		// 403 demo
		addRoute("GET", "/403", MyController, forbid);

		// File upload demo
		addRoute("GET", "/upload", MyController, uploadForm);
		addRoute("POST", "/upload", MyController, upload);
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
		server->registerController(new MyController());
		server->setOption("enable_directory_listing", "true");
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
