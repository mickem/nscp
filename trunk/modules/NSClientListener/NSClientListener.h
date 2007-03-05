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
#include <Socket.h>
#include <string>
#include <utils.h>

NSC_WRAPPERS_MAIN();

class NSClientListener  : public simpleSocket::ListenerHandler {
private:
	simpleSocket::Listener<> socket;
	socketHelpers::allowedHosts allowedHosts;

public:
	NSClientListener();
	virtual ~NSClientListener();
	// Module calls
	bool loadModule();
	bool unloadModule();


	std::string getModuleName() {
		return "NSClient server";
	}
	NSCModuleWrapper::module_version getModuleVersion() {
		NSCModuleWrapper::module_version version = {0, 0, 1 };
		return version;
	}
	std::string getModuleDescription() {
		return "A simple server that listens for incoming NSClient (check_nt) connection and handles them.\nAlthough NRPE is the preferred method NSClient is fully supported and can be used for simplicity or for compatibility.";
	}

	std::string parseRequest(std::string buffer);
	void sendTheResponse(simpleSocket::Socket *client, std::string response);
	void retrivePacket(simpleSocket::Socket *client);
	bool isPasswordOk(std::string remotePassword);

	// simpleSocket::ListenerHandler implementation
	void onAccept(simpleSocket::Socket *client);
	void onClose();
	std::string getConfigurationMeta();

};