#pragma once
#include "resource.h"
#include <Thread.h>
#include <Mutex.h>
#include <WinSock2.h>
#include <Socket.h>
#include <string.h>
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

class NRPESocket : public simpleSocket::Listener {
private:

public:
	NRPESocket();
	virtual ~NRPESocket();

private:
	virtual void onAccept(simpleSocket::Socket client);
};




