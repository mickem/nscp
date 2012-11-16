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
#include "CheckMKServer.h"
#include <strEx.h>
#include <time.h>
#include "handler_impl.hpp"

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;


CheckMKServer::CheckMKServer() {
}
CheckMKServer::~CheckMKServer() {}

bool CheckMKServer::loadModule() {
	return false;
}

bool CheckMKServer::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		root_ = get_core()->getBasePath();
		nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
		lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
		lua_runtime_->register_plugin(boost::shared_ptr<check_mk::check_mk_plugin>(new check_mk::check_mk_plugin()));
		scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));
		handler_.reset(new handler_impl(scripts_));

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("check_mk"), alias, _T("server"));

		settings.alias().add_path_to_settings()
			(_T("CHECK MK SERVER SECTION"), _T("Section for check_mk (CheckMKServer.dll) protocol options."))

			(_T("scripts"), sh::fun_values_path(boost::bind(&CheckMKServer::add_script, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))

			;

		settings.alias().add_key_to_settings()
			(_T("port"), sh::string_key(&info_.port_, "6556"),
			_T("PORT NUMBER"), _T("Port to use for check_mk."))

			;

		settings.alias().add_parent(_T("/settings/default")).add_key_to_settings()

			(_T("thread pool"), sh::uint_key(&info_.thread_pool_size, 10),
			_T("THREAD POOL"), _T(""), true)

			(_T("bind to"), sh::string_key(&info_.address),
			_T("BIND TO ADDRESS"), _T("Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses."), true)

			(_T("socket queue size"), sh::int_key(&info_.back_log, 0),
			_T("LISTEN QUEUE"), _T("Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts."), true)

			(_T("allowed hosts"), sh::string_fun_key<std::wstring>(boost::bind(&socket_helpers::allowed_hosts_manager::set_source, &info_.allowed_hosts, _1), _T("127.0.0.1")),
			_T("ALLOWED HOSTS"), _T("A comaseparated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges."))

			(_T("cache allowed hosts"), sh::bool_key(&info_.allowed_hosts.cached, true),
			_T("CACHE ALLOWED HOSTS"), _T("If hostnames should be cached, improves speed and security somewhat but wont allow you to have dynamic IPs for your nagios server."))

			(_T("timeout"), sh::uint_key(&info_.timeout, 30),
			_T("TIMEOUT"), _T("Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out."))

			(_T("use ssl"), sh::bool_key(&info_.ssl.enabled, false),
			_T("ENABLE SSL ENCRYPTION"), _T("This option controls if SSL should be enabled."), true)

			(_T("dh"), sh::path_key(&info_.ssl.dh_key, "${certificate-path}/nrpe_dh_512.pem"),
			_T("DH KEY"), _T(""), true)

			(_T("certificate"), sh::path_key(&info_.ssl.certificate),
			_T("SSL CERTIFICATE"), _T(""), false)

			(_T("certificate key"), sh::path_key(&info_.ssl.certificate_key),
			_T("SSL CERTIFICATE"), _T(""), true)

			(_T("certificate format"), sh::string_key(&info_.ssl.certificate_format, "PEM"),
			_T("CERTIFICATE FORMAT"), _T(""), true)

			(_T("ca"), sh::path_key(&info_.ssl.ca_path, "${certificate-path}/ca.pem"),
			_T("CA"), _T(""), true)

			(_T("allowed ciphers"), sh::string_key(&info_.ssl.allowed_ciphers, "ADH"),
			_T("ALLOWED CIPHERS"), _T("A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"), false)

			(_T("verify mode"), sh::string_key(&info_.ssl.verify_mode, "none"),
			_T("VERIFY MODE"), _T(""), false)
			;

		settings.register_all();
		settings.notify();

		if (scripts_->empty()) {
			add_script(_T("default"),_T("default_check_mk.lua"));
		}

#ifndef USE_SSL
		if (info_.use_ssl) {
			NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
			return false;
		}
#endif
		NSC_LOG_ERROR_LISTW(info_.validate());

		std::list<std::string> errors;
		info_.allowed_hosts.refresh(errors);
		NSC_LOG_ERROR_LISTS(errors);
		NSC_DEBUG_MSG_STD(_T("Allowed hosts definition: ") + info_.allowed_hosts.to_wstring());

		boost::asio::io_service io_service_;

		scripts_->load_all();


		if (mode == NSCAPI::normalStart) {
			server_.reset(new check_mk::server::server(info_, handler_));
			if (!server_) {
				NSC_LOG_ERROR_STD(_T("Failed to create server instance!"));
				return false;
			}
			server_->start();
		}
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + to_wstring(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
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
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN>"));
		return false;
	}
	return true;
}

bool CheckMKServer::add_script(std::wstring alias, std::wstring file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = _T("");
		}

		boost::optional<boost::filesystem::wpath> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile)
			return false;
		NSC_DEBUG_MSG_STD(_T("Adding script: ") + ofile->string() + _T(" as ") + alias + _T(")"));
		scripts_->add(utf8::cvt<std::string>(alias), utf8::cvt<std::string>(ofile->string()));
		return true;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not load script: (Unknown exception) ") + file);
	}
	return false;
}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(CheckMKServer)
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_IGNORE_CMD_DEF()
