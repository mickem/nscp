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
//#include "stdafx.h"
#include <SSLSocket.h>


void* simpleSSL::Crypto::malloc(int num)
{
	return OPENSSL_malloc(num);
}
void simpleSSL::Crypto::free(void *addr)
{
	return OPENSSL_free(addr);
}
int simpleSSL::Crypto::getNumberOfLocks() {
	return CRYPTO_num_locks();
}

void simpleSSL::Crypto::setLockingCallback(void (*locking_function)(int mode,int type, const char *file,int line)) {
	CRYPTO_set_locking_callback(locking_function);
}
void simpleSSL::Crypto::setIDCallback(unsigned long (*id_function)(void)) {
	CRYPTO_set_id_callback(id_function);
}

void simpleSSL::SSL_init() {
	SSL_load_error_strings();
	SSL_library_init();
}
void simpleSSL::SSL_deinit() {
	/*
	SSL_library_init, leaks memory but apparently OPEN-SSL is crap, so there is not much to do AFAIK! :)
	EVP_cleanup(); 
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state(0);
	*/
	ERR_free_strings();
}

void simpleSSL::count_socket(bool add) {
	static int count = 0;
	if (add) {
		count++;
		std::wcout << "+++SSSL::Socket" << count << std::endl;
	} else {
		count--;
		std::wcout << "---SSSL::Socket" << count << std::endl;
	}
}


bool simpleSSL::sSSL::readAll(simpleSocket::Socket *report_to, simpleSocket::DataBuffer &buffer, unsigned int tmpBufferLength /* = 1024*/) {
	char *tmpBuffer = new char[tmpBufferLength+1];
	if (!ssl_)
		create();
	int n= SSL_read(ssl_,tmpBuffer,tmpBufferLength);
	if (n > 0) {
		buffer.append(tmpBuffer, n);
	}
	delete [] tmpBuffer;
	if (n <= 0) {
		int rc = getError(n);
		if ((rc == SSL_ERROR_WANT_READ) || (rc == SSL_ERROR_WANT_WRITE))
			return true;
		report_to->printError(_T(__FILE__), __LINE__, _T("Could not read from socket: ") + strEx::itos(rc));
		throw simpleSocket::SocketException(_T("Could not read from socket: ") + strEx::itos(rc));
	}
	return n>0;
}
void simpleSSL::sSSL::send(const char * buf, unsigned int len) {
	if (!ssl_)
		create();
	int rc = SSL_write(ssl_, buf, len);
	if (rc <= 0)
		throw SSLException(_T("Socket write failed: "), rc, getError(rc));
}

bool simpleSSL::Listener::accept(tSocket &client) {
	client.setContext(context);
	return simpleSocket::Socket::accept(client);
}



static HANDLE *lock_cs = NULL;


void locking_function(int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		WaitForSingleObject(lock_cs[n], INFINITE);
	}
	else {
		ReleaseMutex(lock_cs[n]);
	}
}

void setupDH(simpleSSL::DH &dh) {
	unsigned char dh512_p[] = {
		0xCF, 0xFF, 0x65, 0xC2, 0xC8, 0xB4, 0xD2, 0x68, 0x8C, 0xC1, 0x80, 0xB1,
			0x7B, 0xD6, 0xE8, 0xB3, 0x62, 0x59, 0x62, 0xED, 0xA7, 0x45, 0x6A, 0xF8,
			0xE9, 0xD8, 0xBE, 0x3F, 0x38, 0x42, 0x5F, 0xB2, 0xA5, 0x36, 0x03, 0xD3,
			0x06, 0x27, 0x81, 0xC8, 0x9B, 0x88, 0x50, 0x3B, 0x82, 0x3D, 0x31, 0x45,
			0x2C, 0xB4, 0xC5, 0xA5, 0xBE, 0x6A, 0xE3, 0x2E, 0xA6, 0x86, 0xFD, 0x6A,
			0x7E, 0x1E, 0x6A, 0x73,
	};
	unsigned char dh512_g[] = { 0x02, };

	dh.bin2bn_p(dh512_p, sizeof(dh512_p));
	dh.bin2bn_g(dh512_g, sizeof(dh512_g));
}

void simpleSSL::Listener::StartListener(std::wstring host, int port, unsigned int listenQue) {
	// @todo init SSL
	simpleSSL::SSL_init();

	context.createSSLv23();
	context.setCipherList();
	simpleSSL::DH dh;
	dh.create();
	setupDH(dh);
	context.setTmpDH(dh.getDH());
	dh.free();

	if (!lock_cs) {
		lock_cs_count = simpleSSL::Crypto::getNumberOfLocks();
		lock_cs = reinterpret_cast<HANDLE*>(simpleSSL::Crypto::malloc(lock_cs_count * sizeof(HANDLE)));
		if (!lock_cs)
			throw simpleSocket::SocketException(_T("Could not create SSL handles."));
		for (int i = 0; i < lock_cs_count; i++) {
			lock_cs[i] = CreateMutex(NULL, FALSE, NULL);
		}
		simpleSSL::Crypto::setLockingCallback(locking_function);
	}
	tBase::StartListener(host, port, listenQue);
}
void simpleSSL::Listener::StopListener() {
	tBase::StopListener();

	context.destroy();
	if (lock_cs) {
		simpleSSL::Crypto::setLockingCallback(NULL);

		for (int i = 0; i < lock_cs_count; i++)
			CloseHandle(lock_cs[i]);
		simpleSSL::Crypto::free(lock_cs);
	}
	simpleSSL::SSL_deinit();
}
