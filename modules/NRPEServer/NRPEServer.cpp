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
#include "NRPEServer.h"
#include <strEx.h>
#include <time.h>
#include "handler_impl.hpp"

#include <settings/client/settings_client.hpp>
#include <socket/socket_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace sh = nscapi::settings_helper;


NRPEServer::NRPEServer() : handler_(new handler_impl(1024)) {
}
NRPEServer::~NRPEServer() {}

bool NRPEServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

	try {
		if (server_) {
			server_->stop();
			server_.reset();
		}
	} catch (...) {
		NSC_LOG_ERROR_STD("Failed to stop server");
		return false;
	}

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("NRPE", alias, "server");


	bool insecure;
	settings.alias().add_key_to_settings()
		("insecure", sh::bool_key(&insecure, false),
		"ALLOW INSECURE CHIPHERS and ENCRYPTION", "Only enable this if you are using legacy check_nrpe client.")
		;

	settings.register_all();
	settings.notify();


	settings.alias().add_path_to_settings()
		("NRPE SERVER SECTION", "Section for NRPE (NRPEServer.dll) (check_nrpe) protocol options.")
		;

	settings.alias().add_key_to_settings()
		("port", sh::string_key(&info_.port_, "5666"),
		"PORT NUMBER", "Port to use for NRPE.")

		("payload length", sh::int_fun_key<unsigned int>(boost::bind(&nrpe::server::handler::set_payload_length, handler_, _1), 1024),
		"PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.", true)

		("allow arguments", sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_allow_arguments, handler_, _1), false),
		"COMMAND ARGUMENT PROCESSING", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

		("allow nasty characters", sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_allow_nasty_arguments, handler_, _1), false),
		"COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.")

		("extended response", sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_multiple_packets, handler_, _1), true),
		"EXTENDED RESPONSE", "Send more then 1 return packet to allow response to go beyond payload size (requires modified client).")

		("performance data", sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_perf_data, handler_, _1), true),
		"PERFORMANCE DATA", "Send performance data back to nagios (set this to 0 to remove all performance data).", true)

		;

	socket_helpers::settings_helper::add_core_server_opts(settings, info_);
#ifdef USE_SSL
	if (insecure) {
		socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true, "", "", "ADH");
	} else {
		socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true);
	}
#endif

	settings.alias().add_parent("/settings/default").add_key_to_settings()
	
		("encoding", sh::string_key(&handler_->encoding_, ""),
		"NRPE PAYLOAD ENCODING", "", true)
		;

		settings.register_all();
		settings.notify();


#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
		return false;
	}
#endif
	if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {

		if (handler_->get_payload_length() != 1024)
			NSC_DEBUG_MSG_STD("Non-standard buffer length (hope you have recompiled check_nrpe changing #define MAX_PACKETBUFFER_LENGTH = " + strEx::s::xtos(handler_->get_payload_length()));
		NSC_LOG_ERROR_LISTS(info_.validate());

		std::list<std::string> errors;
		info_.allowed_hosts.refresh(errors);
		NSC_LOG_ERROR_LISTS(errors);
		NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

		boost::asio::io_service io_service_;

		server_.reset(new nrpe::server::server(info_, handler_));
		if (!server_) {
			NSC_LOG_ERROR_STD("Failed to create server instance!");
			return false;
		}
		server_->start();
	}
	return true;
}

bool NRPEServer::unloadModule() {
	try {
		if (server_) {
			server_->stop();
			server_.reset();
		}
	} catch (...) {
		NSC_LOG_ERROR_EX("unload");
		return false;
	}
	return true;
}
