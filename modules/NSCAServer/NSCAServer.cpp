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
#include "NSCAServer.h"

#include "handler_impl.hpp"

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;
namespace str = nscp::helpers;

NSCAServer::NSCAServer() : info_(boost::shared_ptr<nsca::server::handler>(new handler_impl(1024))) {}

bool NSCAServer::loadModule() {
	return false;
}

bool NSCAServer::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NSCA"), alias, _T("server"));

		settings.alias().add_path_to_settings()
			(_T("NSCA SERVER SECTION"), _T("Section for NSCA (NSCAServer) (check_nsca) protocol options."))
			;

		settings.alias().add_key_to_settings()
			(_T("port"), sh::uint_key(&info_.port, 5667),
			_T("PORT NUMBER"), _T("Port to use for NSCA."))

			(_T("payload length"), sh::int_fun_key<unsigned int>(boost::bind(&nsca::server::handler::set_payload_length, info_.request_handler, _1), 512),
			_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NSCA agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCA agent to use the same value for it to work."))

			(_T("performance data"), sh::bool_fun_key<bool>(boost::bind(&nsca::server::handler::set_perf_data, info_.request_handler, _1), true),
			_T("PERFORMANCE DATA"), _T("Send performance data back to nagios (set this to 0 to remove all performance data)."))

			(_T("encryption"), sh::string_fun_key<std::string>(boost::bind(&nsca::server::handler::set_encryption, info_.request_handler, _1), "aes"),
			_T("ENCRYPTION"), _T("Encryption to use"))

			(_T("password"), sh::string_fun_key<std::string>(boost::bind(&nsca::server::handler::set_password, info_.request_handler, _1), ""),
			_T("PASSWORD"), _T("Password to use"))


			;

		settings.alias().add_parent(_T("/settings/default")).add_key_to_settings()

			(_T("thread pool"), sh::uint_key(&info_.thread_pool_size, 10),
			_T("THREAD POOL"), _T(""))

			(_T("bind to"), sh::string_key(&info_.address),
			_T("BIND TO ADDRESS"), _T("Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses."))

			(_T("socket queue size"), sh::int_key(&info_.back_log, 0),
			_T("LISTEN QUEUE"), _T("Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts."))

			(_T("allowed hosts"), sh::string_fun_key<std::wstring>(boost::bind(&socket_helpers::allowed_hosts_manager::set_source, &info_.allowed_hosts, _1), _T("127.0.0.1")),
			_T("ALLOWED HOSTS"), _T("A comaseparated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges."))

			(_T("cache allowed hosts"), sh::bool_key(&info_.allowed_hosts.cached, true),
			_T("CACHE ALLOWED HOSTS"), _T("If hostnames should be cached, improves speed and security somewhat but wont allow you to have dynamic IPs for your nagios server."))

			(_T("timeout"), sh::uint_key(&info_.timeout, 30),
			_T("TIMEOUT"), _T("Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out."))

			(_T("inbox"), sh::string_fun_key<std::wstring>(boost::bind(&nsca::server::handler::set_channel, info_.request_handler, _1), _T("inbox")),
			_T("INBOX"), _T("The default channel to post incoming messages on"))

			;

		settings.register_all();
		settings.notify();

		if (info_.request_handler->get_payload_length() != 512)
			NSC_DEBUG_MSG_STD(_T("Non-standard buffer length (hope you have recompiled check_nsca changing #define MAX_PACKETBUFFER_LENGTH = ") + strEx::itos(info_.request_handler->get_payload_length()));

		std::list<std::string> errors;
		info_.allowed_hosts.refresh(errors);
		BOOST_FOREACH(const std::string &e, errors) {
			NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(e));
		}
		NSC_DEBUG_MSG_STD(_T("Allowed hosts definition: ") + info_.allowed_hosts.to_wstring());

		if (mode == NSCAPI::normalStart) {
			server_.reset(new nsca::server::nsca_server(info_));
			if (!server_) {
				NSC_LOG_ERROR_STD(_T("Failed to create server instance!"));
				return false;
			}
			server_->setup();
			server_->start();
		}
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + str::to_wstring(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
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
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN>"));
		return false;
	}
	return true;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(NSCAServer);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
