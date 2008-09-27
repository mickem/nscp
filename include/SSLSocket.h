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
#pragma once

#include <Socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <strEx.h>

namespace simpleSSL {
	void count_socket(bool add);

	class SSLException {
	private:
		std::wstring error_;
	public:
		SSLException(std::wstring s) : error_(s) {
		}

		SSLException(std::wstring s, int err) : error_(s){
			error_ += strEx::itos(err);
		}
		
		SSLException(std::wstring s, int err1, int err2) : error_(s){
			error_ += _T("[") + strEx::itos(err1) + _T("] ");
			error_ += strEx::itos(err2);
		}
		
		std::wstring getMessage() {
			return error_;
		}
	};

	class DH {
	private:
		::DH *internalDH;
	public:
		DH() : internalDH(NULL) {
		}
		~DH() {
			if (internalDH)
				DH_free(internalDH);
			internalDH = NULL;
		}
		inline void free() {
			if (internalDH)
				DH_free(internalDH);
			internalDH = NULL;
		}

		inline void create() {
			if (!internalDH)
				internalDH = DH_new();
			if (!internalDH)
				throw SSLException(_T("DH_new failed."));
		}

		inline void bin2bn_p(const unsigned char *s,int len) {
			if (!internalDH)
				throw SSLException(_T("DH_new failed."));

			if (internalDH->p)
				throw SSLException(_T("internalDH->p already exists."));
			internalDH->p = BN_bin2bn(s, len, NULL);
			if (!internalDH->p)
				throw SSLException(_T("internalDH->p failed."));
		}
		inline void bin2bn_g(const unsigned char *s,int len) {
			if (!internalDH)
				throw SSLException(_T("DH_new failed."));

			if (internalDH->g)
				throw SSLException(_T("internalDH->g already exists."));
			internalDH->g = BN_bin2bn(s, len, NULL);
			if (!internalDH->g)
				throw SSLException(_T("internalDH->g failed."));
		}
		inline ::DH* getDH() {
			return internalDH;
		}
	};

	class Context {
		SSL_CTX *ctx_;
	public:
		Context() : ctx_(NULL){
		}
		// @todo Need to make this RAII! (smart pointers ?)
		Context(Context &other) {
			ctx_ = other.ctx_;
		}
		~Context() {
		}

		void destroy() {
			if (ctx_ == NULL)
				throw SSLException(_T("Error: SSL Context not initalized."));
			SSL_CTX_free(ctx_);
			ctx_ = NULL;
		}
		void createSSLv23Server() {
			if (ctx_ != NULL)
				throw SSLException(_T("Error: SSL Context already initalized."));
			ctx_ = SSL_CTX_new(SSLv23_server_method());
			if (ctx_ == NULL) {
				throw SSLException(_T("Error: could not create SSL context."));
			}
		}
		void createSSLv23Client() {
			if (ctx_ != NULL)
				throw SSLException(_T("Error: SSL Context already initalized."));
			ctx_ = SSL_CTX_new(SSLv23_client_method());
			if (ctx_ == NULL)
				throw SSLException(_T("Error: could not create SSL context."));
			SSL_CTX_set_options(ctx_,SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
		}
		void setCipherList(std::string s = "ADH") {
			if (ctx_ == NULL)
				throw SSLException(_T("Error: SSL Context already initalized."));
			SSL_CTX_set_cipher_list(ctx_, s.c_str());
		}
		void setTmpDH(::DH* dh) {
			if (!ctx_)
				throw SSLException(_T("setTmpDH:: Invalid context"));
			if (!dh)
				throw SSLException(_T("setTmpDH:: Invalid dh"));
			SSL_CTX_set_tmp_dh(ctx_, dh);
		}
		SSL* newSSL() {
			return SSL_new(ctx_);
		}


	};

	class sSSL {
	private:
		::SSL *ssl_;
		simpleSSL::Context context_;

	public:
		sSSL() : ssl_(NULL) {
		}
		sSSL(sSSL &other) : ssl_(NULL) {
			ssl_ = other.ssl_;
			other.ssl_ = NULL;
			context_ = other.context_;
		}
		~sSSL() {
			free();
		}
		void free() {
			if (ssl_ == NULL)
				return;
			SSL_free(ssl_);
			ssl_ = NULL;
		}
		void clear() {
			if (!ssl_)
				create();
			int rc = SSL_clear(ssl_);
			if (rc == 0)
				throw SSLException(_T("Error: SSL_clear - failed: "), rc, getError(rc));
		}
		void create() {
			if (ssl_ != NULL) 
				throw SSLException(_T("Error: Could not create SSL connection structure, SSL is already inatialized."));
			ssl_ = context_.newSSL();
			if (ssl_ == NULL) 
				throw SSLException(_T("Error: Could not create SSL connection structure."));
		}
		int getError(int errorCode) {
			if (!ssl_)
				create();
			return SSL_get_error(ssl_, errorCode);
		}
		void accept() {
			if (!ssl_)
				create();
			/**/
			int rc = 0;
			int i = 0;
			while ((rc = SSL_accept(ssl_)) != 1) {
				if (++i >= 100) {
					throw SSLException(_T("SSL: Could not complete SSL handshake."));
				}
				int rc2 = getError(rc);
				if ((rc2 == SSL_ERROR_WANT_READ) || (rc2 == SSL_ERROR_WANT_WRITE)) {
					Sleep(100);
					continue;
				} else {
					throw SSLException(_T("Error: Could not complete SSL handshake : "), rc, rc2);
				}
			}
			/**/
		}
		void connect() {
			if (!ssl_)
				create();
			int rc = 0;
			int i = 0;
			while ((rc = SSL_connect(ssl_)) != 1) {
				if (++i >= 100) {
					throw SSLException(_T("SSL: Could not complete SSL handshake."));
				}
				int rc2 = getError(rc);
				if ((rc2 == SSL_ERROR_WANT_READ) || (rc2 == SSL_ERROR_WANT_WRITE)) {
					Sleep(100);
					continue;
				} else {
					throw SSLException(_T("Error: Could not complete SSL handshake : "), rc, rc2);
				}
			}
			/**/
		}
		void shutdown() {
			if (ssl_ == NULL)
				return;
			int i = 0;
			int rc = 0;
			SSL_shutdown(ssl_);
			// @bug This will break but I don't know how to fix it...
//			SSL_shutdown(ssl_); @bug this leaks memory!
			/*
			while ((rc = SSL_shutdown(ssl_)) != 1) {
				if (++i >= 100) {
					throw SSLException("SSL: Could not complete SSL shutdown.");
				}
				int rc2 = getError(rc);
				if ((rc2 == SSL_ERROR_WANT_READ) || (rc2 == SSL_ERROR_WANT_WRITE)) {
					Sleep(100);
					continue;
				} else {
					throw SSLException("Error: Could not complete SSL shutdown : ", rc, rc2);
				}
			}
			*/
		}
		int set_fd(int fd) {
			if (!ssl_)
				create();
			return SSL_set_fd(ssl_, fd);
		}
		void setContext(Context context) {
			context_ = context;
		}
		bool readAll (simpleSocket::DataBuffer &buffer, unsigned int tmpBufferLength = 1024, int maxLength = -1);
		bool sendAll(const char * buffer, unsigned int len);
		void send(const char * buf, unsigned int len);
	};

	class Socket : public simpleSocket::Socket {
	private:
		typedef simpleSocket::Socket tBase;
		simpleSSL::sSSL ssl;
	public:
		Socket() {
		}
		Socket(Socket &other) : tBase(other), ssl(other.ssl) {
		}
		Socket(bool create) : tBase(create) {
		}
		virtual ~Socket() {
			ssl.shutdown();
			ssl.free();
		}
		void attach(SOCKET s) {
			if (!s)
				throw SSLException(_T("attach:: Invalid socket"));
			try {
				tBase::attach(s);
				tBase::setNonBlock();
				int rc = ssl.set_fd(static_cast<int>(socket_));
				if (rc != 1)
					throw simpleSocket::SocketException(_T("Error: Could not do SSL_set_fd: ") + strEx::itos(rc) + _T(":")  + strEx::itos(rc));
				ssl.accept();
			} catch (simpleSSL::SSLException e) {
				throw simpleSocket::SocketException(e.getMessage());
			}
		}
		virtual bool readAll (simpleSocket::DataBuffer &buffer, unsigned int tmpBufferLength = 1024, int maxLength = -1) {
			try {
				return ssl.readAll(buffer, tmpBufferLength, maxLength);
			} catch (simpleSSL::SSLException e) {
				throw simpleSocket::SocketException(e.getMessage());
			} catch (...) {
				throw simpleSocket::SocketException(_T("Unhandeled socket exception"));
			}
		}
		virtual bool sendAll(const char * buffer, unsigned int len) {
			try {
				return ssl.sendAll(buffer, len);
			} catch (simpleSSL::SSLException e) {
				throw simpleSocket::SocketException(e.getMessage());
			} catch (...) {
				throw simpleSocket::SocketException(_T("Unhandeled socket exception"));
			}
		}
		/*

		virtual int send(const char * buf, unsigned int len, int flags = 0) {
			try {
				ssl.send(buf, len);
			} catch (simpleSSL::SSLException e) {
				throw simpleSocket::SocketException(e.getMessage());
			}
			return 0;
		}
		*/
		virtual void close() {
			ssl.shutdown();
			ssl.free();
			/* @todo
			try {
				ssl.shutdown();
			} catch (simpleSSL::SSLException e) {
				throw simpleSocket::SocketException(e.getMessage());
			}
			*/
			tBase::close();
		}
		virtual int connect(std::wstring host, u_short port) {
			return tBase::connect(host, port);
		}

		virtual int connect_();
		void setContext(Context c) {
			ssl.setContext(c);
		}


	};


	class Listener : public simpleSocket::Listener<simpleSSL::Socket> {
	private:
		typedef simpleSSL::Socket tSocket;
		typedef simpleSocket::Listener<tSocket> tBase;
	public:
		Context context;
		int lock_cs_count;
	public:

		virtual bool accept(tBase::tBase &client);

		void setContext(Context c) {
			context = c;
		}
		virtual void StartListener(std::wstring host, int port, unsigned int listenQue);
		virtual void StopListener();
	};



	namespace Crypto {
		void* malloc(int num);
		void free(void* addr);
		int getNumberOfLocks();
		void setIDCallback(unsigned long (*id_function)(void));
		void setLockingCallback(void (*locking_function)(int mode,int type, const char *file,int line));


	};


	void SSL_init();
	void SSL_deinit();

}



