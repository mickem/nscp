#include "stdafx.h"
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
	SSL_library_init();
	SSL_load_error_strings();
}
void simpleSSL::SSL_deinit() {
	ERR_free_strings();
}



void simpleSSL::SSL::readAll(simpleSocket::DataBuffer &buffer, unsigned int tmpBufferLength /* = 1024*/) {
	// @todo this could be optimized a bit if we want to
	// If only one buffer is needed we could "reuse" the buffer instead of copying it.
	char *tmpBuffer = new char[tmpBufferLength+1];
	if (!ssl_)
		create();
	int n= SSL_read(ssl_,tmpBuffer,tmpBufferLength);
	while (n>0) {
		if (n == tmpBufferLength) {
			// We filled the buffer (There is more to get)
			buffer.append(tmpBuffer, n);
			n=SSL_read(ssl_,tmpBuffer,tmpBufferLength);
		} else {
			// Buffer not full, we got it "all"
			buffer.append(tmpBuffer, n);
			break;
		}
	}
	delete [] tmpBuffer;
	if (n <= 0) {
		int rc = getError(n);
		if ((rc != SSL_ERROR_WANT_READ) && (rc != SSL_ERROR_WANT_WRITE))
			throw SSLException("Socket read failed: ", n, rc);
	}
}
void simpleSSL::SSL::send(const char * buf, unsigned int len) {
	std::cout << "sending data..." << std::endl;
	if (!ssl_)
		create();
	int rc = SSL_write(ssl_, buf, len);
	if (rc <= 0)
		throw SSLException("Socket write failed: ", rc, getError(rc));
}

bool simpleSSL::Listener::accept(tSocket &client) {
	client.setContext(context);
	if (!simpleSocket::Socket::accept(client))
		return false;
	return true;
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

void simpleSSL::Listener::StartListener(int port) {
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
			throw simpleSocket::SocketException("Could not create SSL handles.");
		for (int i = 0; i < lock_cs_count; i++) {
			lock_cs[i] = CreateMutex(NULL, FALSE, NULL);
		}
		simpleSSL::Crypto::setLockingCallback(locking_function);
	}

	tBase::StartListener(port);
}
void simpleSSL::Listener::StopListener() {
	tBase::StopListener();

	context.destroy();
	if (!lock_cs)
		return;

	simpleSSL::Crypto::setLockingCallback(NULL);

	for (int i = 0; i < lock_cs_count; i++)
		CloseHandle(lock_cs[i]);
	simpleSSL::Crypto::free(lock_cs);
	simpleSSL::SSL_deinit();
}
