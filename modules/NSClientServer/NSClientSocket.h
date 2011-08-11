#pragma once
#include "resource.h"
#include <Socket.h>
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


