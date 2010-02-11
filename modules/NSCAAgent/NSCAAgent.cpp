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
#include "NSCAAgent.h"
#include <utils.h>
#include <list>
#include <string>
#include <nsca/nsca_enrypt.hpp>
#include <nsca/nsca_packet.hpp>
#include <nsca/nsca_socket.hpp>

NSCAAgent gNSCAAgent;

/**
 * DLL Entry point
 * @param hModule 
 * @param ul_reason_for_call 
 * @param lpReserved 
 * @return 
 */
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

/**
 * Default c-tor
 * @return 
 */
NSCAAgent::NSCAAgent() {}
/**
 * Default d-tor
 * @return 
 */
NSCAAgent::~NSCAAgent() {}
/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool NSCAAgent::loadModule(NSCAPI::moduleLoadMode mode) {
	try {

		SETTINGS_REG_PATH(nsca::SECTION);
		SETTINGS_REG_PATH(nsca::SERVER_SECTION);
		SETTINGS_REG_PATH(nsca::CMD_SECTION);

		SETTINGS_REG_KEY_S(nsca::HOSTNAME);
		SETTINGS_REG_KEY_S(nsca::SERVER_HOST);
		SETTINGS_REG_KEY_I(nsca::SERVER_PORT);
		SETTINGS_REG_KEY_I(nsca::ENCRYPTION);
		SETTINGS_REG_KEY_S(nsca::PASSWORD);
		SETTINGS_REG_KEY_B(nsca::CACHE_HOST);

		hostname_ = to_string(SETTINGS_GET_STRING(nsca::HOSTNAME));
		nscahost_ = SETTINGS_GET_STRING(nsca::SERVER_HOST);
		nscaport_ = SETTINGS_GET_INT(nsca::SERVER_PORT);

		encryption_method_ = SETTINGS_GET_INT(nsca::ENCRYPTION);
		password_ = strEx::wstring_to_string(SETTINGS_GET_STRING(nsca::PASSWORD));
		cacheNscaHost_ = SETTINGS_GET_INT(nsca::CACHE_HOST);
		timeout_ = SETTINGS_GET_INT(nsca::READ_TIMEOUT);
		payload_length_ = SETTINGS_GET_INT(nsca::PAYLOAD_LENGTH);
		time_delta_ = strEx::stol_as_time_sec(SETTINGS_GET_STRING(nsca::TIME_DELTA_DEFAULT), 1);


	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}


	return true;
}
/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool NSCAAgent::unloadModule() {
	return true;
}
std::wstring NSCAAgent::getCryptos() {
	std::wstring ret = _T("{");
	for (int i=0;i<LAST_ENCRYPTION_ID;i++) {
		if (nsca::nsca_encrypt::hasEncryption(i)) {
			std::wstring name;
			try {
				nsca::nsca_encrypt::any_encryption *core = nsca::nsca_encrypt::get_encryption_core(i);
				if (core == NULL)
					name = _T("Broken<NULL>");
				else
					name = core->getName();
			} catch (nsca::nsca_encrypt::encryption_exception &e) {
				name = e.getMessage();
			}
			if (ret.size() > 1)
				ret += _T(", ");
			ret += strEx::itos(i) + _T("=") + name;
		}
	}
	return ret + _T("}");
}

NSCAPI::nagiosReturn NSCAAgent::handleSimpleNotification(const std::wstring channel, const std::wstring command, NSCAPI::nagiosReturn code, std::wstring msg, std::wstring perf) {
	try {
		NSC_DEBUG_MSG_STD(_T("* * *NSCA * * * Handling command: ") + command);
		boost::asio::io_service io_service;
		NSC_DEBUG_MSG_STD(_T("* * *NSCA * * * message: ") + msg);
		nsca::socket socket(io_service);
		socket.connect(nscahost_, nscaport_);
		nsca::packet packet(hostname_, payload_length_, time_delta_);
		packet.code = code;
		packet.host = "hello";
		packet.result = to_string(msg);
		socket.recv_iv(password_, encryption_method_, boost::posix_time::seconds(timeout_));
		socket.send_nsca(packet, boost::posix_time::seconds(timeout_));
		return 1;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: ") + to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to send data: UNKNOWN"));
	}
}


NSC_WRAPPERS_MAIN_DEF(gNSCAAgent);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF(gNSCAAgent);
