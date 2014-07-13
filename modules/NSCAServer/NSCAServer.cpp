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
#include "NSCAServer.h"

#include "handler_impl.hpp"

#include <socket/socket_settings_helper.hpp>
#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;

NSCAServer::NSCAServer() : handler_(new nsca_handler_impl(1024)) {}



bool NSCAServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {

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
	settings.set_alias("NSCA", alias, "server");

	settings.alias().add_path_to_settings()
		("NSCA SERVER SECTION", "Section for NSCA (NSCAServer) (check_nsca) protocol options.")
		;

	settings.alias().add_key_to_settings()
		("port", sh::string_key(&info_.port_, "5667"),
		"PORT NUMBER", "Port to use for NSCA.")

		("payload length", sh::int_fun_key<unsigned int>(boost::bind(&nsca::server::handler::set_payload_length, handler_, _1), 512),
		"PAYLOAD LENGTH", "Length of payload to/from the NSCA agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCA agent to use the same value for it to work.")

		("performance data", sh::bool_fun_key<bool>(boost::bind(&nsca::server::handler::set_perf_data, handler_, _1), true),
		"PERFORMANCE DATA", "Send performance data back to nagios (set this to false to remove all performance data).")

		("encryption", sh::string_fun_key<std::string>(boost::bind(&nsca::server::handler::set_encryption, handler_, _1), "aes"),
		"ENCRYPTION", std::string("Name of encryption algorithm to use.\nHas to be the same as your agent i using or it wont work at all."
			"This is also independent of SSL and generally used instead of SSL.\nAvailable encryption algorithms are:\n") + nscp::encryption::helpers::get_crypto_string("\n"))

		;

	socket_helpers::settings_helper::add_core_server_opts(settings, info_);
	socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, false);

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("password", sh::string_fun_key<std::string>(boost::bind(&nsca::server::handler::set_password, handler_, _1), ""),
		"PASSWORD", "Password to use")

		("inbox", sh::string_fun_key<std::string>(boost::bind(&nsca::server::handler::set_channel, handler_, _1), "inbox"),
		"INBOX", "The default channel to post incoming messages on")

		;

	settings.register_all();
	settings.notify();


#ifndef USE_SSL
	if (info_.ssl.enabled) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
		return false;
	}
#endif
	if (handler_->get_payload_length() != 512)
		NSC_DEBUG_MSG_STD("Non-standard buffer length (hope you have recompiled check_nsca changing #define MAX_PACKETBUFFER_LENGTH = " + strEx::s::xtos(handler_->get_payload_length()));
	NSC_LOG_ERROR_LISTS(info_.validate());

	std::list<std::string> errors;
	info_.allowed_hosts.refresh(errors);
	NSC_LOG_ERROR_LISTS(errors);
	NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

	if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {

		server_.reset(new nsca::server::server(info_, handler_));
		if (!server_) {
			NSC_LOG_ERROR_STD("Failed to create server instance!");
			return false;
		}
		server_->start();
	}
	return true;
}

bool NSCAServer::unloadModule() {
	try {
		if (server_) {
			server_->stop();
			server_.reset();
		}
	} catch (...) {
		NSC_LOG_ERROR_STD("Exception caught: <UNKNOWN>");
		return false;
	}
	return true;
}
