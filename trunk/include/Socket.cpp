#include "stdafx.h"
#include <Socket.h>
#include <NSCHelper.h>

/**
 * Print an error message 
 * @param error 
 */
void simpleSocket::Socket::printError(std::string error) {
	NSC_LOG_ERROR_STD(error);
}


/**
 * Read all data on the socket
 * @param buffer 
 * @param tmpBufferLength Length of temporary buffer to use (generally larger then expected data)
 */
void simpleSocket::Socket::readAll(DataBuffer& buffer, unsigned int tmpBufferLength /* = 1024*/) {
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


/**
 * Startup WSA
 * @param wVersionRequested Version to use
 * @return stuff
 */
WSADATA simpleSocket::WSAStartup(WORD wVersionRequested /* = 0x202 */) {
	WSADATA wsaData;
	int wsaret=::WSAStartup(wVersionRequested,&wsaData);
	if(wsaret != 0)
		throw SocketException("WSAStartup failed: " + strEx::itos(wsaret));
	return wsaData;
}
/**
 * Cleanup (Shutdown) WSA
 */
void simpleSocket::WSACleanup() {
	if (::WSACleanup() != 0)
		throw SocketException("WSACleanup failed: ", ::WSAGetLastError());
}
