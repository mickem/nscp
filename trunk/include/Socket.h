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
class SimpleSocketListsner {
private:
	MutexHandler mutexHandler;
	SOCKET server;
	HANDLE hStopEvent;
	u_short port_;

public:
	SimpleSocketListsner(u_short port) : port_(port), hStopEvent(NULL) {};
	virtual ~SimpleSocketListsner();
	DWORD threadProc(LPVOID lpParameter);
	void exitThread(void);
#define RECV_BUFFER_LEN 1024
	typedef std::pair<char*,unsigned int> readAllDataBlock;
	static readAllDataBlock readAll(SOCKET socket) {
		// @bug Is this even working ?
		// @todo Nedds *alot* more work...
		unsigned int buffLen = RECV_BUFFER_LEN;
		char *retBuf = NULL;
		char *buff = new char[buffLen];
		int n=recv(socket,buff,RECV_BUFFER_LEN,0);
		while ((n!=SOCKET_ERROR )||(n!=0)) {
			if (n == RECV_BUFFER_LEN) {
				char* newBuf = new char[buffLen+RECV_BUFFER_LEN];
				memcpy(newBuf, buff, buffLen);
				n = recv(socket, buff, RECV_BUFFER_LEN, 0);
				if ((n!=SOCKET_ERROR )&&(n!=0)) {
					memcpy(&newBuf[buffLen], buff, n);
					buffLen += n;
				}
				if (retBuf)
					delete [] retBuf;
				retBuf = newBuf;
			} else {
				buffLen = n;
				break;
			}
		}
		if (retBuf) {
			delete [] buff;
			return readAllDataBlock(retBuf,buffLen);
		}
		return readAllDataBlock(buff,buffLen);
	}


private:
	virtual void onAccept(SOCKET client) {}
};


