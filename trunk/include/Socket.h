#pragma once
#include "resource.h"
#include <Thread.h>
#include <Mutex.h>
#include <WinSock2.h>

namespace simpleSocket {
	class SocketException {
	private:
		std::string error_;
	public:
		SocketException(std::string error) : error_(error) {}
		SocketException(std::string error, int errorCode) : error_(error) {
			error_ += strEx::itos(errorCode);
		}
		std::string getMessage() const {
			return error_;
		}
		
	};
	class DataBuffer {
	private:
		char *buffer_;
		unsigned int length_;
	public:
		DataBuffer() : buffer_(NULL), length_(0){
		}
		DataBuffer(const DataBuffer &other) {
			buffer_ = new char[other.getLength()];
			memcpy(buffer_, other.getBuffer(), other.getLength());
			length_ = other.getLength();
		}
		virtual ~DataBuffer() {
			delete [] buffer_;
			length_ = 0;
		}
		void append(const char* buffer, const unsigned int length) {
			char *tBuf = new char[length_+length+1];
			memcpy(tBuf, buffer_, length_);
			memcpy(&tBuf[length_], buffer, length);
			delete [] buffer_;
			buffer_ = tBuf;
			length_ += length;
		}
		const char * getBuffer() const {
			return buffer_;
		}
		unsigned int getLength() const {
			return length_;
		}
	};

	class Socket {
	private:
		SOCKET socket_;
		sockaddr_in from_;

	public:
		Socket() : socket_(NULL) {
		}
		Socket(SOCKET socket) : socket_(socket) {
		}
		Socket(Socket &other) {
			socket_ = other.socket_;
			other.socket_ = NULL;
		}
		virtual ~Socket() {
			if (socket_)
				closesocket(socket_);
			socket_ = NULL;
		}
		SOCKET getSocket() const {
			return socket_;
		}
		virtual void close() {
			assert(socket_);
			closesocket(socket_);
			socket_ = NULL;
		}
		void readAll(DataBuffer &buffer, unsigned int tmpBufferLength = 1024);

		void socket(int af, int type, int protocol ) {
			socket_ = ::socket(af, type, protocol);
			assert(socket_ != INVALID_SOCKET);
		}
		void bind() {
			int fromlen=sizeof(from_);
			if (::bind(socket_, (sockaddr*)&from_, fromlen) == SOCKET_ERROR)
				throw SocketException("bind failed: ", ::WSAGetLastError());
		}
		void listen(int backlog = 0) {
			if (::listen(socket_, backlog) == SOCKET_ERROR)
				throw SocketException("listen failed: ", ::WSAGetLastError());
		}
		bool accept(Socket &client) {
			int fromlen=sizeof(client.from_);
			client.socket_ = ::accept(socket_, (sockaddr*)&client.from_, &fromlen);
			if(client.socket_ == INVALID_SOCKET) {
				int err = ::WSAGetLastError();
				if (err == WSAEWOULDBLOCK)
					return false;
				throw SocketException("accept failed: ", ::WSAGetLastError());
			}
			return true;
		}
		void setAddr(short family, u_long addr, u_short port) {
			from_.sin_family=family;
			from_.sin_addr.s_addr=addr;
			from_.sin_port=port;
		}
		int send(const char * buf, unsigned int len, int flags ) {
			return ::send(socket_, buf, len, flags);
		}
		void ioctlsocket(long cmd, u_long *argp) {
			if (::ioctlsocket(socket_, cmd, argp) == SOCKET_ERROR)
				throw SocketException("ioctlsocket failed: ", ::WSAGetLastError());
		}

		static WSADATA WSAStartup(WORD wVersionRequested = 0x202) {
			WSADATA wsaData;
			int wsaret=::WSAStartup(wVersionRequested,&wsaData);
			if(wsaret != 0)
				throw SocketException("WSAStartup failed: " + strEx::itos(wsaret));
			return wsaData;
		}
		static void WSACleanup() {
			if (::WSACleanup() != 0)
				throw SocketException("WSACleanup failed: ", ::WSAGetLastError());
		}



	};


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
	class Listener : public Socket {
	public:
		class ListenerThread {
		private:
			HANDLE hStopEvent;
		public:
			ListenerThread() : hStopEvent(NULL) {
			}
			DWORD threadProc(LPVOID lpParameter);
			void exitThread(void);
		};

	private:
		MutexHandler mutexHandler;
		u_short port_;
		typedef Thread<ListenerThread> listenThreadManager;
		listenThreadManager threadManager_;

	public:
		Listener() {};
		virtual ~Listener();

		void StartListen(int port);
		virtual void close();

	private:
		virtual void onAccept(Socket client) = 0;

	};
}

