#pragma once
#include "resource.h"
#include <Thread.h>
#include <Mutex.h>
#include <WinSock2.h>
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
class NRPESocket {
private:
	MutexHandler mutexHandler;
	SOCKET server;
	HANDLE hStopEvent;

public:
	NRPESocket();
	virtual ~NRPESocket();
	DWORD threadProc(LPVOID lpParameter);
	void exitThread(void);

private:
	bool isRunning(void);
	void stopRunning(void);
	void startRunning(void);
	std::list<std::string> split(char* buffer);
	std::string parseCommand(char* request);
};
#define DEFAULT_NRPE_PORT 5666



typedef Thread<NRPESocket> NRPESocketThread; // Thread manager


