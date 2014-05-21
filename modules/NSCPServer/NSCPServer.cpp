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
#include "NSCPServer.h"
#include <strEx.h>
#include <time.h>
#include "handler_impl.hpp"

#include <settings/client/settings_client.hpp>
#include <socket/socket_settings_helper.hpp>

namespace sh = nscapi::settings_helper;


NSCPServer::NSCPServer() : handler_(new handler_impl()) {
}
NSCPServer::~NSCPServer() {}

bool NSCPServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("nscp", alias, "server");

	settings.alias().add_path_to_settings()
		("NSCP SERVER SECTION", "Section for NSCP (NSCPListener.dll) (check_nscp) protocol options.")
		;

	settings.alias().add_key_to_settings()
		("port", sh::string_key(&info_.port_, "5668"),
		"PORT NUMBER", "Port to use for NSCP.")

		("allow arguments", sh::bool_fun_key<bool>(boost::bind(&handler_impl::set_allow_arguments, handler_, _1), false),
		"COMMAND ARGUMENT PROCESSING", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

		;

	socket_helpers::settings_helper::add_core_server_opts(settings, info_);
	socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true);

	settings.register_all();
	settings.notify();


#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
		return false;
	}
#endif

	boost::asio::io_service io_service_;

	if (mode == NSCAPI::normalStart) {

		NSC_LOG_ERROR_LISTS(info_.validate());

		std::list<std::string> errors;
		info_.allowed_hosts.refresh(errors);
		NSC_LOG_ERROR_LISTS(errors);
		NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

		server_.reset(new nscp::server::server(info_, handler_));
		if (!server_) {
			NSC_LOG_ERROR_STD("Failed to create server instance!");
			return false;
		}
		server_->start();
	}
	return true;
}

bool NSCPServer::unloadModule() {
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
