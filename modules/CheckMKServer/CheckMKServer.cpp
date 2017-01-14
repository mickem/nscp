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

#include "CheckMKServer.h"
#include "handler_impl.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <socket/socket_settings_helper.hpp>

#include <str/xtos.hpp>
#include <time.h>

namespace sh = nscapi::settings_helper;

CheckMKServer::CheckMKServer() {}
CheckMKServer::~CheckMKServer() {}

bool CheckMKServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	root_ = get_base_path();
	nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
	lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
	lua_runtime_->register_plugin(boost::shared_ptr<check_mk::check_mk_plugin>(new check_mk::check_mk_plugin()));
	scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));
	handler_.reset(new handler_impl(scripts_));

	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias("check_mk", alias, "server");

	settings.alias().add_path_to_settings()
		("CHECK MK SERVER SECTION", "Section for check_mk (CheckMKServer.dll) protocol options.")

		("scripts", sh::fun_values_path(boost::bind(&CheckMKServer::add_script, this, _1, _2)),
			"REMOTE TARGET DEFINITIONS", "",
			"TARGET", "For more configuration options add a dedicated section")

		;

	settings.alias().add_key_to_settings()
		("port", sh::string_key(&info_.port_, "6556"),
			"PORT NUMBER", "Port to use for check_mk.")

		;

	socket_helpers::settings_helper::add_core_server_opts(settings, info_);
	socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, false);

	settings.register_all();
	settings.notify();

	if (scripts_->empty()) {
		add_script("default", "default_check_mk.lua");
	}

#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
		return false;
	}
#endif
	NSC_LOG_ERROR_LISTS(info_.validate());

	std::list<std::string> errors;
	info_.allowed_hosts.refresh(errors);
	NSC_LOG_ERROR_LISTS(errors);
	NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

	boost::asio::io_service io_service_;

	scripts_->load_all();

	if (mode == NSCAPI::normalStart) {
		server_.reset(new check_mk::server::server(info_, handler_));
		if (!server_) {
			NSC_LOG_ERROR_STD("Failed to create server instance!");
			return false;
		}
		server_->start();
	}
	return true;
}

bool CheckMKServer::unloadModule() {
	try {
		if (server_) {
			server_->stop();
			server_.reset();
		}
		scripts_.reset();
		lua_runtime_.reset();
		nscp_runtime_.reset();
		handler_.reset();
	} catch (...) {
		NSC_LOG_ERROR_EX("unload");
		return false;
	}
	return true;
}

bool CheckMKServer::add_script(std::string alias, std::string file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = "";
		}

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile)
			return false;
		NSC_DEBUG_MSG_STD("Adding script: " + ofile->string());
		scripts_->add(alias, ofile->string());
		return true;
	} catch (...) {
		NSC_LOG_ERROR_EX("add script");
	}
	return false;
}