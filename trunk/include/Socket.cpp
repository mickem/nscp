#include "stdafx.h"
#include <Socket.h>
#include <NSCHelper.h>




/**
* Thread procedure for the socket listener
* @param lpParameter Potential argument to the thread proc.
* @return thread exit status
* @todo This needs to be reworked, possibly completely redone ?
*/


void simpleSocket::Socket::printError(std::string error) {
	NSC_LOG_ERROR_STD(error);
}


void simpleSocket::Socket::readAll(DataBuffer &buffer, unsigned int tmpBufferLength /* = 1024*/) {
	// @todo this could be optimized a bit if we want to
	// If only one buffer is needed we could "reuse" the buffer instead of copying it.
	char *tmpBuffer = new char[tmpBufferLength+1];
	int n=recv(socket_,tmpBuffer,tmpBufferLength,0);
	while ((n!=SOCKET_ERROR )&&(n!=0)) {
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


WSADATA simpleSocket::WSAStartup(WORD wVersionRequested /* = 0x202 */) {
	WSADATA wsaData;
	int wsaret=::WSAStartup(wVersionRequested,&wsaData);
	if(wsaret != 0)
		throw SocketException("WSAStartup failed: " + strEx::itos(wsaret));
	return wsaData;
}
void simpleSocket::WSACleanup() {
	if (::WSACleanup() != 0)
		throw SocketException("WSACleanup failed: ", ::WSAGetLastError());
}
