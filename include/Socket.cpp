#include "stdafx.h"
#include <Socket.h>


simpleSocket::Listener::~Listener() {
	// @todo: Force cleanup here
}


/**
* Thread procedure for the socket listener
* @param lpParameter Potential argument to the thread proc.
* @return thread exit status
* @todo This needs to be reworked, possibly completely redone ?
*/
DWORD simpleSocket::Listener::ListenerThread::threadProc(LPVOID lpParameter)
{
	unsigned long NoBlock = 1;
	Listener *core = reinterpret_cast<Listener*>(lpParameter);

	hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent) {
		NSC_LOG_ERROR_STD("Create StopEvent failed: " + strEx::itos(GetLastError()));
		return 0;
	}

	try {
		core->socket(AF_INET,SOCK_STREAM,0);
		core->setAddr(AF_INET, INADDR_ANY, htons(core->port_));
		core->bind();
		core->listen(10);
		core->ioctlsocket(FIONBIO, &NoBlock);

		NSC_DEBUG_MSG_STD("Currently listeneing to: " + strEx::itos(core->port_));

		while (!(WaitForSingleObject(hStopEvent, 100) == WAIT_OBJECT_0)) {
			Socket client;
			if (core->accept(client))
				core->onAccept(client);
		}
	} catch (SocketException e) {
		NSC_LOG_ERROR_STD(e.getMessage());
	}

	CloseHandle(hStopEvent);
	NSC_DEBUG_MSG("Socket closed!");
	return 0;
}

/**
* Exit thread callback proc. 
* This is called by the thread manager when the thread should initiate a shutdown procedure.
* The thread manager is responsible for waiting for the actual termination of the thread.
*/
void simpleSocket::Listener::ListenerThread::exitThread(void) {
	NSC_DEBUG_MSG("Requesting Socket shutdown!");
	if (!SetEvent(hStopEvent)) {
		NSC_LOG_ERROR_STD("SetStopEvent failed");
	}
}

void simpleSocket::Socket::readAll(DataBuffer &buffer, unsigned int tmpBufferLength /* = 1024*/) {
	// @todo this could be optimized a bit if we want to
	// If only one buffer is needed we could "reuse" the buffer instead of copying it.
	char *tmpBuffer = new char[tmpBufferLength+1];
	int n=recv(socket_,tmpBuffer,tmpBufferLength,0);
	while ((n!=SOCKET_ERROR )||(n!=0)) {
		if (n == tmpBufferLength) {
			// We filled the buffer (There is more to get)
			buffer.append(tmpBuffer, n);
			n=recv(socket_,tmpBuffer,tmpBufferLength,0);

		} else {
			// Buffer not full, we got it "all"
			buffer.append(tmpBuffer, n);
			break;
		}
	}
	delete [] tmpBuffer;
}

void simpleSocket::Listener::StartListen(int port) {
	port_ = port;
	threadManager_.createThread(this);
}
void simpleSocket::Listener::close() {
	threadManager_.exitThread();
	Socket::close();
}




