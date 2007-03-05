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

NSC_WRAPPERS_MAIN();
#include <Socket.h>
#include <SSLSocket.h>
#include <map>
#include "NRPEPacket.h"

class NRPEListener : public simpleSocket::ListenerHandler {
private:
	typedef enum {
		inject, script, script_dir,
	} command_type;
	struct command_data {
		command_data() : type(inject) {}
		command_data(command_type type_, std::string arguments_) : type(type_), arguments(arguments_) {}
		command_type type;
		std::string arguments;
	};
	bool bUseSSL_;
	simpleSSL::Listener socket_ssl_;
	simpleSocket::Listener<> socket_;
	typedef std::map<strEx::blindstr, command_data> command_list;
	command_list commands;
	unsigned int timeout;
	socketHelpers::allowedHosts allowedHosts;
	bool noPerfData_;
	std::string scriptDirectory_;

public:
	NRPEListener();
	virtual ~NRPEListener();
	// Module calls
	bool loadModule();
	bool unloadModule();


	std::string getModuleName() {
		return "NRPE server";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::string getModuleDescription() {
		return "A simple server that listens for incoming NRPE connection and handles them.\nNRPE is preferred over NSClient as it is more flexible. You can of cource use both NSClient and NRPE.";
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleCommand(const strEx::blindstr command, const unsigned int argLen, char **char_args, std::string &message, std::string &perf);
	std::string getConfigurationMeta();

private:
	class NRPEException {
		std::string error_;
	public:
/*		NRPESocketException(simpleSSL::SSLException e) {
			error_ = e.getMessage();
		}
		NRPEException(NRPEPacket::NRPEPacketException e) {
			error_ = e.getMessage();
		}
		*/
		NRPEException(std::string s) {
			error_ = s;
		}
		std::string getMessage() {
			return error_;
		}
	};


private:
	void onAccept(simpleSocket::Socket *client);
	void onClose();


	NRPEPacket handlePacket(NRPEPacket p);
	int executeNRPECommand(std::string command, std::string &msg, std::string &perf);
	void addAllScriptsFrom(std::string path);
	void addCommand(command_type type, strEx::blindstr key, std::string args = "") {
		addCommand(key, command_data(type, args));
	}
	void addCommand(strEx::blindstr key, command_data args) {
		commands[key] = args;
	}

};

