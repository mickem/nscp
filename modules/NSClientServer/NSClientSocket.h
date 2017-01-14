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

#pragma once
#include "resource.h"
#include <Socket.h>
class NSClientSocket : public simpleSocket::Listener {
private:
	strEx::splitList allowedHosts_;

public:
	NSClientSocket();
	virtual ~NSClientSocket();

private:
	virtual void onAccept(simpleSocket::Socket client);
	std::string parseRequest(std::string buffer);
	bool inAllowedHosts(std::string s) {
		if (allowedHosts_.empty())
			return true;
		strEx::splitList::const_iterator cit;
		for (cit = allowedHosts_.begin();cit!=allowedHosts_.end();++cit) {
			if ( (*cit) == s)
				return true;
		}
		return false;
	}

public:
	void setAllowedHosts(strEx::splitList allowedHosts) {
		allowedHosts_ = allowedHosts;
	}

};


#define REQ_CLIENTVERSION	1	// Works fine!
#define REQ_CPULOAD			2	// Quirks
#define REQ_UPTIME			3	// Works fine!
#define REQ_USEDDISKSPACE	4	// Works fine!
#define REQ_SERVICESTATE	5	// Works fine!
#define REQ_PROCSTATE		6	// Works fine!
#define REQ_MEMUSE			7	// Works fine!
//#define REQ_COUNTER		8	// ! - not implemented Have to look at this, if anyone has a sample let me know...
//#define REQ_FILEAGE		9	// ! - not implemented Dont know how to use
//#define REQ_INSTANCES	10	// ! - not implemented Dont know how to use


