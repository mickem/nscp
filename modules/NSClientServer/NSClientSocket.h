/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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


