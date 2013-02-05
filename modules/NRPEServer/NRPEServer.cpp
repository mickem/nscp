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
#include "NRPEServer.h"
#include <strEx.h>
#include <time.h>
#include "handler_impl.hpp"

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;


NRPEServer::NRPEServer() : handler_(new handler_impl(1024)) {
}
NRPEServer::~NRPEServer() {}

bool NRPEServer::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(_T("NRPE"), alias, _T("server"));

	settings.alias().add_path_to_settings()
		(_T("NRPE SERVER SECTION"), _T("Section for NRPE (NRPEServer.dll) (check_nrpe) protocol options."))
		;

	settings.alias().add_key_to_settings()
		(_T("port"), sh::string_key(&info_.port_, "5666"),
		_T("PORT NUMBER"), _T("Port to use for NRPE."))

		(_T("payload length"), sh::int_fun_key<unsigned int>(boost::bind(&nrpe::server::handler::set_payload_length, handler_, _1), 1024),
		_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work."), true)

		(_T("allow arguments"), sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_allow_arguments, handler_, _1), false),
		_T("COMMAND ARGUMENT PROCESSING"), _T("This option determines whether or not the we will allow clients to specify arguments to commands that are executed."))

		(_T("allow nasty characters"), sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_allow_nasty_arguments, handler_, _1), false),
		_T("COMMAND ALLOW NASTY META CHARS"), _T("This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments."))

		(_T("performance data"), sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_perf_data, handler_, _1), true),
		_T("PERFORMANCE DATA"), _T("Send performance data back to nagios (set this to 0 to remove all performance data)."), true)

		;

	settings.alias().add_parent(_T("/settings/default")).add_key_to_settings()

		(_T("thread pool"), sh::uint_key(&info_.thread_pool_size, 10),
		_T("THREAD POOL"), _T("Size of socket responder thread pool (number of simultaneous connection)"), true)

		(_T("bind to"), sh::string_key(&info_.address),
		_T("BIND TO ADDRESS"), _T("Force server to bind to a given network interface (specify IP address here). Leaving this blank will bind to all available IP addresses."), true)

		(_T("socket queue size"), sh::int_key(&info_.back_log, 0),
		_T("LISTEN QUEUE"), _T("Number of connection to queue before starting to refuse new incoming connections."), true)

		(_T("allowed hosts"), sh::string_fun_key<std::wstring>(boost::bind(&socket_helpers::allowed_hosts_manager::set_source, &info_.allowed_hosts, _1), _T("127.0.0.1")),
		_T("ALLOWED HOSTS"), _T("A coma separated list of hosts which are allowed to connect. You can use netmasks (/ syntax) or * to create ranges."))

		(_T("cache allowed hosts"), sh::bool_key(&info_.allowed_hosts.cached, true),
		_T("CACHE ALLOWED HOSTS"), _T("If hostnames should be cached, improves speed and security somewhat but wont allow you to have dynamic IPs for your nagios server."))

		(_T("timeout"), sh::uint_key(&info_.timeout, 30),
		_T("TIMEOUT"), _T("Timeout for incoming connections."))

		(_T("use ssl"), sh::bool_key(&info_.ssl.enabled, true),
		_T("ENABLE SSL ENCRYPTION"), _T("Encrypt connection with SSL (has to match client configuration)."), false)

		(_T("dh"), sh::path_key(&info_.ssl.dh_key, "${certificate-path}/nrpe_dh_512.pem"),
		_T("DH KEY"), _T("The Diffie-Hellman key to use for key exchange. Set to none to disable pre created keys and generate keys on the fly"), true)

		(_T("certificate"), sh::path_key(&info_.ssl.certificate),
		_T("SSL CERTIFICATE"), _T("Certificate file to use. Set to non to disable certificates."), false)

		(_T("certificate key"), sh::path_key(&info_.ssl.certificate_key),
		_T("SSL CERTIFICATE"), _T("Key for certificate file to use."), true)

		(_T("certificate format"), sh::string_key(&info_.ssl.certificate_format, "PEM"),
		_T("CERTIFICATE FORMAT"), _T("Formats of certificates pem or asn1."), true)

		(_T("ca"), sh::path_key(&info_.ssl.ca_path, "${certificate-path}/ca.pem"),
		_T("CA"), _T("Certificate authority to use when validating certificates."), true)

		(_T("allowed ciphers"), sh::string_key(&info_.ssl.allowed_ciphers, "ADH"),
		_T("ALLOWED CIPHERS"), _T("The list of allowed chipers default is (for legacy reasons UNSECURE ADH) a better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"), false)

		(_T("verify mode"), sh::string_key(&info_.ssl.verify_mode, "none"),
		_T("VERIFY MODE"), _T("Which verification mode to use for remote certificates (none, peer, peer-cert, etc)"), false)
	
		(_T("encoding"), sh::string_key(&handler_->encoding_, ""),
		_T("NRPE PAYLOAD ENCODING"), _T(""), true)
		;

		settings.register_all();
		settings.notify();


#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
		return false;
	}
#endif
	if (handler_->get_payload_length() != 1024)
		NSC_DEBUG_MSG_STD(_T("Non-standard buffer length (hope you have recompiled check_nrpe changing #define MAX_PACKETBUFFER_LENGTH = ") + strEx::itos(handler_->get_payload_length()));
	NSC_LOG_ERROR_LISTW(info_.validate());

	std::list<std::string> errors;
	info_.allowed_hosts.refresh(errors);
	NSC_LOG_ERROR_LISTS(errors);
	NSC_DEBUG_MSG_STD(_T("Allowed hosts definition: ") + info_.allowed_hosts.to_wstring());

	boost::asio::io_service io_service_;

	if (mode == NSCAPI::normalStart) {
		server_.reset(new nrpe::server::server(info_, handler_));
		if (!server_) {
			NSC_LOG_ERROR_STD(_T("Failed to create server instance!"));
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
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN>"));
		return false;
	}
	return true;
}
