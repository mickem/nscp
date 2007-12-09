/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include <Socket.h>
#include <NSCHelper.h>

/**
 * Print an error message 
 * @param error 
 */
void simpleSocket::Socket::printError(std::wstring FILE, int LINE, std::wstring error) {
	NSCModuleHelper::Message(NSCAPI::error, FILE, LINE, error);
}


/**
 * Read all data on the socket
 * @param buffer 
 * @param tmpBufferLength Length of temporary buffer to use (generally larger then expected data)
 */
bool simpleSocket::Socket::readAll(DataBuffer& buffer, unsigned int tmpBufferLength /* = 1024*/, int maxLength /* = -1 */) {
	// @todo this could be optimized a bit if we want to
	// If only one buffer is needed we could "reuse" the buffer instead of copying it.
	char *tmpBuffer = new char[tmpBufferLength+1];
	int n=recv(socket_,tmpBuffer,tmpBufferLength,0);
	std::wcout << _T("read: ") << n << std::endl;
	while ((n!=SOCKET_ERROR )&&(n!=0)) {
		if (n == tmpBufferLength) {
			// We filled the buffer (There is more to get)
			buffer.append(tmpBuffer, n);
			if ((maxLength!=-1)&&(n >= maxLength))
				break;
			n=recv(socket_,tmpBuffer,tmpBufferLength,0);
		} else {
			// Buffer not full, we got it "all"
			buffer.append(tmpBuffer, n);
			break;

		}
	}
	delete [] tmpBuffer;
	if (n == SOCKET_ERROR) {
		int ret = ::WSAGetLastError();
		if (ret == WSAEWOULDBLOCK)
			return true;
		throw SocketException(_T("recv returned SOCKET_ERROR: "), ret);
	}
	return n>0;
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
		throw SocketException(_T("WSAStartup failed: ") + strEx::itos(wsaret));
	return wsaData;
}
/**
 * Cleanup (Shutdown) WSA
 */
void simpleSocket::WSACleanup() {
	if (::WSACleanup() != 0)
		throw SocketException(_T("WSACleanup failed: "), ::WSAGetLastError());
}
