#pragma once
#include "resource.h"
#include <Thread.h>
#include <Mutex.h>
#include <WinSock2.h>
#include <strEx.h>
#include <charEx.h>
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
class NSClientSocket {
private:
	MutexHandler mutexHandler;
	SOCKET server;
	HANDLE hStopEvent;

public:
	NSClientSocket();
	virtual ~NSClientSocket();
	DWORD threadProc(LPVOID lpParameter);
	void exitThread(void);

private:
	bool isRunning(void);
	void stopRunning(void);
	void startRunning(void);
	std::list<std::string> split(char* buffer);
	std::string parseCommand(char* request);

	void onAccept(SOCKET client);
	std::string parseRequest(char *buffer);
};

#define DEFAULT_TCP_PORT 12489

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


typedef Thread<NSClientSocket> NSClientSocketThread; // Thread manager


