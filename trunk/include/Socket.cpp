#include "stdafx.h"
#include <Socket.h>


SimpleSocketListsner::~SimpleSocketListsner() {
	// @todo: Force cleanup here
}


/**
* Thread procedure for the socket listener
* @param lpParameter Potential argument to the thread proc.
* @return thread exit status
* @todo This needs to be reworked, possibly completely redone ?
*/
DWORD SimpleSocketListsner::threadProc(LPVOID lpParameter)
{
	hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent) {
		NSC_LOG_ERROR_STD("Create StopEvent failed: " + strEx::itos(GetLastError()));
		return 0;
	}

	WSADATA wsaData;
	sockaddr_in local;
	int wsaret=WSAStartup(0x101,&wsaData);
	if(wsaret!=0) {
		NSC_LOG_ERROR_STD("WSA Startup failed: " + strEx::itos(wsaret));
		return 0;
	}

	local.sin_family=AF_INET;
	local.sin_addr.s_addr=INADDR_ANY;
	local.sin_port=htons(port_);
	server=socket(AF_INET,SOCK_STREAM,0);
	if(server==INVALID_SOCKET) {
		WSACleanup();
		NSC_LOG_ERROR_STD("Could not create listening socket: " + strEx::itos(GetLastError()));
		return 0;
	}

	if(bind(server,(sockaddr*)&local,sizeof(local))!=0) {
		closesocket(server);
		WSACleanup();
		NSC_LOG_ERROR_STD("Could not bind socket: " + strEx::itos(GetLastError()));
		return 0;
	}

	if(listen(server,10)!=0) {
		closesocket(server);
		WSACleanup();
		NSC_LOG_ERROR_STD("Could not open socket: " + strEx::itos(GetLastError()));
		return 0;
	}

	SOCKET client;
	sockaddr_in from;
	int fromlen=sizeof(from);
	while (!(WaitForSingleObject(hStopEvent, 100) == WAIT_OBJECT_0)) {
		client=accept(server, (struct sockaddr*)&from,&fromlen);
		if (client != INVALID_SOCKET)
			onAccept(client);
	}
	closesocket(server);
	WSACleanup();
	NSC_DEBUG_MSG("Socket closed!");
	return 0;
}

/**
* Exit thread callback proc. 
* This is called by the thread manager when the thread should initiate a shutdown procedure.
* The thread manager is responsible for waiting for the actual termination of the thread.
*/
void SimpleSocketListsner::exitThread(void) {
	NSC_DEBUG_MSG("Requesting Socket shutdown!");
	if (!SetEvent(hStopEvent)) {
		NSC_LOG_ERROR_STD("SetStopEvent failed");
	}
}






