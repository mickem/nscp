/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "NSCAServer.h"

#include <socket/socket_settings_helper.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_common_options.hpp>
#include <nscapi/macros.hpp>

namespace sh = nscapi::settings_helper;

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

		("payload length", sh::uint_key(&payload_length_, 512),
			"PAYLOAD LENGTH", "Length of payload to/from the NSCA agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCA agent to use the same value for it to work.")

		("performance data", sh::bool_fun_key(boost::bind(&NSCAServer::set_perf_data, this, _1), true),
			"PERFORMANCE DATA", "Send performance data back to nagios (set this to false to remove all performance data).")

		("encryption", sh::string_fun_key(boost::bind(&NSCAServer::set_encryption, this, _1), "aes"),
			"ENCRYPTION", std::string("Name of encryption algorithm to use.\nHas to be the same as your agent i using or it wont work at all."
				"This is also independent of SSL and generally used instead of SSL.\nAvailable encryption algorithms are:\n") + nscp::encryption::helpers::get_crypto_string("\n"))

		;

	socket_helpers::settings_helper::add_core_server_opts(settings, info_);
	socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, false);

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("password", sh::string_key(&password_, ""),
			DEFAULT_PASSWORD_NAME, DEFAULT_PASSWORD_DESC)

		("inbox", sh::string_key(&channel_, "inbox"),
			"INBOX", "The default channel to post incoming messages on")

		;

	settings.register_all();
	settings.notify();

#ifndef USE_SSL
	if (info_.ssl.enabled) {
		NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
		return false;
	}
#endif
	if (payload_length_ != 512)
		NSC_DEBUG_MSG_STD("Non-standard buffer length (hope you have recompiled check_nsca changing #define MAX_PACKETBUFFER_LENGTH = " + str::xtos(payload_length_));
	NSC_LOG_ERROR_LISTS(info_.validate());

	std::list<std::string> errors;
	info_.allowed_hosts.refresh(errors);
	NSC_LOG_ERROR_LISTS(errors);
	NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());
	NSC_DEBUG_MSG_STD("Starting server on: " + info_.to_string());

	if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {
		server_.reset(new nsca::server::server(info_, this));
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

void NSCAServer::handle(nsca::packet p) {
	std::string response;
	std::string::size_type pos = p.result.find('|');
	nscapi::core_helper helper(get_core(), get_id());
	if (pos != std::string::npos) {
		std::string msg = p.result.substr(0, pos);
		std::string perf = p.result.substr(++pos);
		helper.submit_simple_message(channel_, p.host, "", p.service, nscapi::plugin_helper::int2nagios(p.code), msg, perf, response);
	} else {
		std::string empty, msg = p.result;
		helper.submit_simple_message(channel_, p.host, "", p.service, nscapi::plugin_helper::int2nagios(p.code), msg, empty, response);
	}
}