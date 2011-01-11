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
#include <config.h>
#include <msvc_wrappers.h>
#include "handler_impl.hpp"

namespace sh = nscapi::settings_helper;

NRPEListener gNRPEListener;

NRPEListener::NRPEListener() : info_(boost::shared_ptr<nrpe::server::handler>(new handler_impl(1024))) {
}
NRPEListener::~NRPEListener() {}

std::wstring getAllowedHosts() {
	return SETTINGS_GET_STRING_FALLBACK(nrpe::ALLOWED_HOSTS, protocol_def::ALLOWED_HOSTS);
}
bool getCacheAllowedHosts() {
	return SETTINGS_GET_BOOL_FALLBACK(nrpe::CACHE_ALLOWED, protocol_def::CACHE_ALLOWED);
}



bool NRPEListener::loadModule() {
	return false;
}

bool NRPEListener::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

/*
DEFINE_SETTING_S(ALLOWED_HOSTS, NRPE_SECTION_PROTOCOL, GENERIC_KEY_ALLOWED_HOSTS, "");
DESCRIBE_SETTING(ALLOWED_HOSTS, "ALLOWED HOST ADDRESSES", "This is a comma-delimited list of IP address of hosts that are allowed to talk to NSClient deamon. If you leave this blank the global version will be used instead.");

DEFINE_SETTING_B(CACHE_ALLOWED, NRPE_SECTION_PROTOCOL, GENERIC_KEY_SOCK_CACHE_ALLOWED, false);
DESCRIBE_SETTING_ADVANCED(CACHE_ALLOWED, "ALLOWED HOSTS CACHING", "Used to cache looked up hosts if you check dynamic/changing hosts set this to false.");
*/
	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NRPE"), alias, _T("server"));

		settings.alias().add_path_to_settings()
			(_T("NRPE SERVER SECTION"), _T("Section for NRPE (NRPEListener.dll) (check_nrpe) protocol options."))
			;

		settings.alias().add_key_to_settings()
			(_T("port"), sh::uint_key(&info_.port, 5666),
			_T("PORT NUMBER"), _T("Port to use for NRPE."))

			(_T("thread pool"), sh::uint_key(&info_.thread_pool_size, 10),
			_T("THREAD POOL"), _T(""))

			(_T("timeout"), sh::uint_key(&info_.timeout, 30),
			_T("TIMEOUT"), _T("Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out."))

			(_T("use ssl"), sh::bool_key(&info_.use_ssl, true),
			_T("ENABLE SSL ENCRYPTION"), _T("This option controls if SSL should be enabled."))

			(_T("payload length"), sh::int_fun_key<unsigned int>(boost::bind(&nrpe::server::handler::set_payload_length, info_.request_handler, _1), 1024),
			_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work."))

			(_T("allow arguments"), sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_allow_arguments, info_.request_handler, _1), false),
			_T("COMMAND ARGUMENT PROCESSING"), _T("This option determines whether or not the we will allow clients to specify arguments to commands that are executed."))

			(_T("allow nasty characters"), sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_allow_nasty_arguments, info_.request_handler, _1), false),
			_T("COMMAND ALLOW NASTY META CHARS"), _T("This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments."))

			(_T("performance data"), sh::bool_fun_key<bool>(boost::bind(&nrpe::server::handler::set_perf_data, info_.request_handler, _1), true),
			_T("PERFORMANCE DATA"), _T("Send performance data back to nagios (set this to 0 to remove all performance data)."))

			(_T("certificate"), sh::wpath_key(&info_.certificate, _T("${certificate-path}/nrpe_dh_512.pem")),
			_T("SSL CERTIFICATE"), _T(""))
			;

		settings.alias().add_parent(_T("/settings/default")).add_key_to_settings()

			(_T("bind to"), sh::string_key(&info_.address),
			_T("BIND TO ADDRESS"), _T("Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses."))

			(_T("socket queue size"), sh::int_key(&info_.back_log, 0),
			_T("LISTEN QUEUE"), _T("Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts."))

			;



		settings.register_all();
		settings.notify();


#ifndef USE_SSL
		if (info_.use_ssl) {
			NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
		}
#endif
		if (info_.request_handler->get_payload_length() != 1024)
			NSC_DEBUG_MSG_STD(_T("Non-standard buffer length (hope you have recompiled check_nrpe changing #define MAX_PACKETBUFFER_LENGTH = ") + strEx::itos(info_.request_handler->get_payload_length()));
		if (!boost::filesystem::is_regular(info_.certificate))
			NSC_LOG_ERROR_STD(_T("Certificate not found: ") + info_.certificate);

		boost::asio::io_service io_service_;

		allowedHosts.setAllowedHosts(strEx::splitEx(getAllowedHosts(), _T(",")), getCacheAllowedHosts(), io_service_);
		NSC_DEBUG_MSG_STD(_T("Allowed hosts: ") + allowedHosts.to_string());

		if (mode == NSCAPI::normalStart) {
			if (info_.use_ssl) {
#ifdef USE_SSL
				server_.reset(new nrpe::server::server(info_));
#else
				NSC_LOG_ERROR_STD(_T("SSL is not supported (not compiled with openssl)"));
				return false;
#endif
			} else {
				server_.reset(new nrpe::server::server(info_));
			}
			if (!server_) {
				NSC_LOG_ERROR_STD(_T("Failed to create server instance!"));
				return false;
			}
			server_->start();
		}
	} catch (nrpe::server::nrpe_exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.what());
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + to_wstring(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}


	return true;
}

bool NRPEListener::unloadModule() {
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


bool NRPEListener::hasCommandHandler() {
	return false;
}
bool NRPEListener::hasMessageHandler() {
	return false;
}




// void NRPEListener::onAccept(simpleSocket::Socket *client) 
// {
// 	if (!allowedHosts.inAllowedHosts(client->getAddr())) {
// 		NSC_LOG_ERROR(_T("Unauthorize access from: ") + client->getAddrString());
// 		client->close();
// 		return;
// 	}
// 	try {
// 		simpleSocket::DataBuffer block;
// 		int i;
// 		int maxWait = socketTimeout_*10;
// 		for (i=0;i<maxWait;i++) {
// 			bool lastReadHasMore = false;
// 			try {
// 				lastReadHasMore = client->readAll(block, 1048);
// 			} catch (simpleSocket::SocketException e) {
// 				NSC_LOG_MESSAGE(_T("Could not read NRPE packet from socket :") + e.getMessage());
// 				client->close();
// 				return;
// 			}
// 			if (block.getLength() >= NRPEPacket::getBufferLength(buffer_length_))
// 				break;
// 			if (!lastReadHasMore) {
// 				NSC_LOG_MESSAGE(_T("Could not read a full NRPE packet from socket, only got: ") + strEx::itos(block.getLength()));
// 				client->close();
// 				return;
// 			}
// 			Sleep(100);
// 		}
// 		if (i >= maxWait) {
// 			NSC_LOG_ERROR_STD(_T("Timeout reading NRPE-packet (increase socket_timeout), we only got: ") + strEx::itos(block.getLength()));
// 			client->close();
// 			return;
// 		}
// 		if (block.getLength() == NRPEPacket::getBufferLength(buffer_length_)) {
// 			try {
// 				NRPEPacket out = handlePacket(NRPEPacket(block.getBuffer(), block.getLength(), buffer_length_));
// 				block.copyFrom(out.getBuffer(), out.getBufferLength());
// 			} catch (NRPEPacket::NRPEPacketException e) {
// 				NSC_LOG_ERROR_STD(_T("NRPESocketException: ") + e.getMessage());
// 				try {
// 					NRPEPacket err(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("Could not construct return paket in NRPE handler check clientside (nsclient.log) logs..."), buffer_length_);
// 					block.copyFrom(err.getBuffer(), err.getBufferLength());
// 				} catch (NRPEPacket::NRPEPacketException e) {
// 					NSC_LOG_ERROR_STD(_T("NRPESocketException (again): ") + e.getMessage());
// 					client->close();
// 					return;
// 				}
// 			}
// 			int maxWait = socketTimeout_*10;
// 			for (i=0;i<maxWait;i++) {
// 				bool lastReadHasMore = false;
// 				try {
// 					if (client->canWrite())
// 						lastReadHasMore = client->sendAll(block);
// 				} catch (simpleSocket::SocketException e) {
// 					NSC_LOG_MESSAGE(_T("Could not send NRPE packet from socket :") + e.getMessage());
// 					client->close();
// 					return;
// 				}
// 				if (!lastReadHasMore) {
// 					client->close();
// 					return;
// 				}
// 				Sleep(100);
// 			}
// 			if (i >= maxWait) {
// 				NSC_LOG_ERROR_STD(_T("Timeout reading NRPE-packet (increase socket_timeout)"));
// 				client->close();
// 				return;
// 			}
// 		} else {
// 			NSC_LOG_ERROR_STD(_T("We got more then we wanted ") + strEx::itos(NRPEPacket::getBufferLength(buffer_length_)) + _T(", we only got: ") + strEx::itos(block.getLength()));
// 			client->close();
// 			return;
// 		}
// 	} catch (simpleSocket::SocketException e) {
// 		NSC_LOG_ERROR_STD(_T("SocketException: ") + e.getMessage());
// 	} catch (NRPEException e) {
// 		NSC_LOG_ERROR_STD(_T("NRPEException: ") + e.getMessage());
// 	} catch (...) {
// 		NSC_LOG_ERROR_STD(_T("Unhandled Exception in NRPE listner..."));
// 	}
// 	client->close();
// }
// 
// NRPEPacket NRPEListener::handlePacket(NRPEPacket p) {
// 	if (p.getType() != NRPEPacket::queryPacket) {
// 		NSC_LOG_ERROR(_T("Request is not a query."));
// 		throw NRPEException(_T("Invalid query type: ") + strEx::itos(p.getType()));
// 	}
// 	if (p.getVersion() != NRPEPacket::version2) {
// 		NSC_LOG_ERROR(_T("Request had unsupported version."));
// 		throw NRPEException(_T("Invalid version"));
// 	}
// 	if (!p.verifyCRC()) {
// 		NSC_LOG_ERROR(_T("Request had invalid checksum."));
// 		throw NRPEException(_T("Invalid checksum"));
// 	}
// 	strEx::token cmd = strEx::getToken(p.getPayload(), '!');
// 	if (cmd.first == _T("_NRPE_CHECK")) {
// 		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnOK, _T("I (") + NSCModuleHelper::getApplicationVersionString() + _T(") seem to be doing fine..."), buffer_length_);
// 	}
// 	std::wstring msg, perf;
// 
// 	if (allowArgs_) {
// 		if (!cmd.second.empty()) {
// 			NSC_LOG_ERROR(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
// 			throw NRPEException(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
// 		}
// 	}
// 	if (allowNasty_) {
// 		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
// 			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
// 			throw NRPEException(_T("Request command contained illegal metachars!"));
// 		}
// 		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
// 			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
// 			throw NRPEException(_T("Request command contained illegal metachars!"));
// 		}
// 	}
// 	//TODO REMOVE THIS
// 	//return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("TEST TEST TEST"), buffer_length_);
// 
// 	NSCAPI::nagiosReturn ret = -3;
// 	try {
// 		ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
// 	} catch (...) {
// 		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("UNKNOWN: Internal exception"), buffer_length_);
// 	}
// 	switch (ret) {
// 		case NSCAPI::returnInvalidBufferLen:
// 			msg = _T("UNKNOWN: Return buffer to small to handle this command.");
// 			ret = NSCAPI::returnUNKNOWN;
// 			break;
// 		case NSCAPI::returnIgnored:
// 			msg = _T("UNKNOWN: No handler for that command");
// 			ret = NSCAPI::returnUNKNOWN;
// 			break;
// 		case NSCAPI::returnOK:
// 		case NSCAPI::returnWARN:
// 		case NSCAPI::returnCRIT:
// 		case NSCAPI::returnUNKNOWN:
// 			break;
// 		default:
// 			msg = _T("UNKNOWN: Internal error.");
// 			ret = NSCAPI::returnUNKNOWN;
// 	}
// 	if (msg.length() >= buffer_length_-1) {
// 		NSC_LOG_ERROR(_T("Truncating returndata as it is bigger then NRPE allowes :("));
// 		msg = msg.substr(0,buffer_length_-2);
// 	}
// 	if (perf.empty()||noPerfData_) {
// 		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg, buffer_length_);
// 	} else {
// 		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg + _T("|") + perf, buffer_length_);
// 	}
// }

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gNRPEListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
