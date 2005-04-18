#pragma once
#include "resource.h"
#include <Socket.h>
#include <SSLSocket.h>

/**
 * @ingroup NSClient++
 * Socket responder class.
 * This is a background thread that listens to the socket and executes incoming commands.
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 * @todo This is not very well written and should probably be reworked.
 *
 * @bug 
 *
 */


#define NASTY_METACHARS         "|`&><'\"\\[]{}"        /* This may need to be modified for windows directory seperator */

typedef short int16_t;
typedef unsigned long u_int32_t;


template <class TBase>
class NRPESocket : public TBase {
private:
	strEx::splitList allowedHosts_;

	class NRPESocketException {
		std::string error_;
	public:
		NRPESocketException(simpleSSL::SSLException e) {
			error_ = e.getMessage();
		}
		NRPESocketException(NRPEPacket::NRPEPacketException e) {
			error_ = e.getMessage();
		}
		NRPESocketException(std::string s) {
			error_ = s;
		}
		std::string getMessage() {
			return error_;
		}
	};


public:
	NRPESocket() {
	}
	virtual ~NRPESocket() {
	}


private:
	NRPEPacket handlePacket(NRPEPacket p) {
		if (p.getType() != NRPEPacket::queryPacket) {
			NSC_LOG_ERROR("Request is not a query.");
			throw NRPESocketException("Invalid query type");
		}
		if (p.getVersion() != NRPEPacket::version2) {
			NSC_LOG_ERROR("Request had unsupported version.");
			throw NRPESocketException("Invalid version");
		}
		if (!p.verifyCRC()) {
			NSC_LOG_ERROR("Request had invalid checksum.");
			throw NRPESocketException("Invalid checksum");
		}
		strEx::token cmd = strEx::getToken(p.getPayload(), '!');
		std::string msg, perf;
		NSC_DEBUG_MSG_STD("Command: " + cmd.first);
		NSC_DEBUG_MSG_STD("Arguments: " + cmd.second);

		if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_ARGUMENTS, NRPE_SETTINGS_ALLOW_ARGUMENTS_DEFAULT) == 0) {
			if (!cmd.second.empty()) {
				NSC_LOG_ERROR("Request contained arguments (not currently allowed).");
				throw NRPESocketException("Request contained arguments (not currently allowed).");
			}
		}
		if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_ALLOW_NASTY_META, NRPE_SETTINGS_ALLOW_NASTY_META_DEFAULT) == 0) {
			if (cmd.first.find_first_of(NASTY_METACHARS) != std::string::npos) {
				NSC_LOG_ERROR("Request command contained illegal metachars!");
				throw NRPESocketException("Request command contained illegal metachars!");
			}
			if (cmd.second.find_first_of(NASTY_METACHARS) != std::string::npos) {
				NSC_LOG_ERROR("Request arguments contained illegal metachars!");
				throw NRPESocketException("Request command contained illegal metachars!");
			}
		}

		NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
		if (perf.empty()) {
			return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg);
		} else {
			return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg + "|" + perf);
		}
	}
	void setupDH(simpleSSL::DH &dh);
};
