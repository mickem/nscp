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

NRPEListener gNRPEListener;

NRPEListener::NRPEListener() : noPerfData_(false), info_(boost::shared_ptr<nrpe::server::handler>(new handler_impl(0))) {
}
NRPEListener::~NRPEListener() {
	std::cout << "TERMINATING TERMINATING!!!" << std::endl;
}

std::wstring getAllowedHosts() {
	return SETTINGS_GET_STRING_FALLBACK(nrpe::ALLOWED_HOSTS, protocol_def::ALLOWED_HOSTS);
}
bool getCacheAllowedHosts() {
	return SETTINGS_GET_BOOL_FALLBACK(nrpe::CACHE_ALLOWED, protocol_def::CACHE_ALLOWED);
}



bool NRPEListener::loadModule(NSCAPI::moduleLoadMode mode) {
	SETTINGS_REG_KEY_I(nrpe::PORT);
	SETTINGS_REG_KEY_S(nrpe::BINDADDR);
	SETTINGS_REG_KEY_I(nrpe::LISTENQUE);
	SETTINGS_REG_KEY_I(nrpe::READ_TIMEOUT);
	SETTINGS_REG_KEY_B(nrpe::KEYUSE_SSL);
	SETTINGS_REG_KEY_I(nrpe::PAYLOAD_LENGTH);
	SETTINGS_REG_KEY_B(nrpe::ALLOW_PERFDATA);
	SETTINGS_REG_KEY_S(nrpe::SCRIPT_PATH);
	SETTINGS_REG_KEY_I(nrpe::CMD_TIMEOUT);
	SETTINGS_REG_KEY_B(nrpe::ALLOW_ARGS);
	SETTINGS_REG_KEY_B(nrpe::ALLOW_NASTY);

	SETTINGS_REG_PATH(nrpe::SECTION);
	SETTINGS_REG_PATH(nrpe::SECTION_HANDLERS);

	info_.use_ssl = SETTINGS_GET_BOOL(nrpe::KEYUSE_SSL)==1;

#ifndef USE_SSL
	if (bUseSSL_) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
	}
#endif

	noPerfData_ = SETTINGS_GET_INT(nrpe::ALLOW_PERFDATA)==0;
	timeout = SETTINGS_GET_INT(nrpe::READ_TIMEOUT);
	info_.request_handler->set_payload_length(SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH));
	if (info_.request_handler->get_payload_length() != 1024)
		NSC_DEBUG_MSG_STD(_T("Non-standard buffer length (hope you have recompiled check_nrpe changing #define MAX_PACKETBUFFER_LENGTH = ") + strEx::itos(info_.request_handler->get_payload_length()));

	boost::asio::io_service io_service_;
	allowedHosts.setAllowedHosts(strEx::splitEx(getAllowedHosts(), _T(",")), getCacheAllowedHosts(), io_service_);
	NSC_DEBUG_MSG_STD(_T("Allowed hosts: ") + allowedHosts.to_string());
	try {
		info_.port = to_string(SETTINGS_GET_INT(nrpe::PORT));
		info_.address = to_string(SETTINGS_GET_STRING(nrpe::BINDADDR));
		unsigned int backLog = SETTINGS_GET_INT(nrpe::LISTENQUE); // @todo: add to info block
		info_.thread_pool_size = 10; // @todo Add as option
		if (mode == NSCAPI::normalStart) {
			if (info_.use_ssl) {
#ifdef USE_SSL
				server_.reset(new nrpe::server::server(info_));
//				NSC_LOG_ERROR_STD(_T("SSL not implemented"));
//				return false;
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
