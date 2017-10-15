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

#include "NRPEServer.h"
#include <str/utils.hpp>
#include <time.h>

#include <socket/socket_settings_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/macros.hpp>

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/macros.hpp>

#include <config.h>

namespace sh = nscapi::settings_helper;

NRPEServer::NRPEServer() {}
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
		("NRPE Server", "Section for NRPE (NRPEServer.dll) (check_nrpe) protocol options.")
		;

	settings.alias().add_key_to_settings()
		("port", sh::string_key(&info_.port_, "5666"),
			"PORT NUMBER", "Port to use for NRPE.")

		("payload length", sh::uint_key(&payload_length_, 1024),
			"PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.", true)

		("allow arguments", sh::bool_key(&allowArgs_, false),
			"COMMAND ARGUMENT PROCESSING", "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

		("allow nasty characters", sh::bool_key(&allowNasty_, false),
			"COMMAND ALLOW NASTY META CHARS", "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.")

		("performance data", sh::bool_fun_key(boost::bind(&NRPEServer::set_perf_data, this, _1), true),
			"PERFORMANCE DATA", "Send performance data back to nagios (set this to 0 to remove all performance data).", true)

		;

	socket_helpers::settings_helper::add_core_server_opts(settings, info_);
#ifdef USE_SSL
	if (insecure) {
		socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true, "", "", "ADH");

		settings.alias().add_key_to_settings()
			("extended response", sh::bool_key(&multiple_packets_, false),
				"EXTENDED RESPONSE", "Send more then 1 return packet to allow response to go beyond payload size (requires modified client if legacy is true this defaults to false).")
			;
	} else {
		socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true);

		settings.alias().add_key_to_settings()
			("extended response", sh::bool_key(&multiple_packets_, true),
				"EXTENDED RESPONSE", "Send more then 1 return packet to allow response to go beyond payload size (requires modified client if legacy is true this defaults to false).")
			;
	}
#endif

	settings.alias().add_parent("/settings/default").add_key_to_settings()

		("encoding", sh::string_key(&encoding_, ""),
			"NRPE PAYLOAD ENCODING", "", true)
		;

	settings.register_all();
	settings.notify();

#ifndef USE_SSL
	if (info_.use_ssl) {
		NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
		return false;
	}
#endif
	if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {
		if (payload_length_ != 1024)
			NSC_DEBUG_MSG_STD("Non-standard buffer length (hope you have recompiled check_nrpe changing #define MAX_PACKETBUFFER_LENGTH = " + str::xtos(payload_length_));
		NSC_LOG_ERROR_LISTS(info_.validate());

		std::list<std::string> errors;
		info_.allowed_hosts.refresh(errors);
		NSC_LOG_ERROR_LISTS(errors);
		NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());
		NSC_DEBUG_MSG_STD("Server config: " + info_.to_string());

		boost::asio::io_service io_service_;

		server_.reset(new nrpe::server::server(info_, this));
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



std::list<nrpe::packet> NRPEServer::handle(nrpe::packet p) {
	std::list<nrpe::packet> packets;
	str::utils::token cmd = str::utils::getToken(p.getPayload(), '!');
	if (cmd.first == "_NRPE_CHECK") {
		packets.push_back(nrpe::packet::create_response(NSCAPI::query_return_codes::returnOK, "I (" + utf8::cvt<std::string>(nscapi::plugin_singleton->get_core()->getApplicationVersionString()) + ") seem to be doing fine...", p.get_payload_length()));
		return packets;
	}
	if (!allowArgs_) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR("Request contained arguments (not currently allowed, check the allow arguments option).");
			throw nrpe::nrpe_exception("Request contained arguments (not currently allowed, check the allow arguments option).");
		}
	}
	if (!allowNasty_) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR("Request command contained illegal metachars!");
			throw nrpe::nrpe_exception("Request command contained illegal metachars!");
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR("Request arguments contained illegal metachars!");
			throw nrpe::nrpe_exception("Request command contained illegal metachars!");
		}
	}
	std::string wmsg, wperf;
	NSCAPI::nagiosReturn ret = -3;
	nscapi::core_helper ch(get_core(), get_id());
	try {
		const unsigned int max_len = p.get_payload_length() - 1;
		std::string wcmd, wargs;
		if (encoding_.empty()) {
			wcmd = utf8::cvt<std::string>(utf8::to_unicode(cmd.first));
			wargs = utf8::cvt<std::string>(utf8::to_unicode(cmd.second));
		} else {
			wcmd = utf8::cvt<std::string>(utf8::from_encoding(cmd.first, encoding_));
			wargs = utf8::cvt<std::string>(utf8::from_encoding(cmd.second, encoding_));
		}
		ret = ch.simple_query_from_nrpe(wcmd, wargs, wmsg, wperf, multiple_packets_?-1:max_len);
		switch (ret) {
		case NSCAPI::query_return_codes::returnOK:
		case NSCAPI::query_return_codes::returnWARN:
		case NSCAPI::query_return_codes::returnCRIT:
		case NSCAPI::query_return_codes::returnUNKNOWN:
			break;
		default:
			throw nrpe::nrpe_exception("UNKNOWN: Internal error.");
		}
		std::string data, msg, perf;
		if (encoding_.empty()) {
			msg = utf8::to_system(utf8::cvt<std::wstring>(wmsg));
			perf = utf8::to_system(utf8::cvt<std::wstring>(wperf));
		} else {
			msg = utf8::to_encoding(utf8::cvt<std::wstring>(wmsg), encoding_);
			perf = utf8::to_encoding(utf8::cvt<std::wstring>(wperf), encoding_);
		}
		if (perf.empty() || noPerfData_) {
			data = msg;
		} else {
			data = msg + "|" + perf;
		}
		if (multiple_packets_) {
			std::size_t data_len = data.size();
			for (std::size_t i = 0; i < data_len; i += max_len) {
				if (data_len - i <= max_len)
					packets.push_back(nrpe::packet::create_response(ret, data.substr(i, max_len), p.get_payload_length()));
				else
					packets.push_back(nrpe::packet::create_more_response(ret, data.substr(i, max_len), p.get_payload_length()));
			}
		} else {
			if (data.length() >= max_len) {
				data = data.substr(0, max_len);
			}
			packets.push_back(nrpe::packet::create_response(ret, data, p.get_payload_length()));
		}
	} catch (...) {
		packets.push_back(nrpe::packet::create_response(NSCAPI::query_return_codes::returnUNKNOWN, "UNKNOWN: Internal exception", p.get_payload_length()));
		return packets;
	}

	return packets;
}