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
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
//#include <windows.h>
#include <WinSock2.h>
#include <Thread.h>
#include <Mutex.h>
#include <error.hpp>
#include <NSCHelper.h>


namespace simpleSocket {
	class SocketException {
	private:
		std::wstring error_;
	public:
		SocketException(std::wstring error) : error_(error) {}
		SocketException(std::wstring error, unsigned int errorCode) : error_(error) {
			error_ += error::format::from_system(errorCode);
		}
		std::wstring getMessage() const {
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
		DataBuffer(const DataBuffer &other) : buffer_(NULL) {
			buffer_ = new char[other.getLength()+2];
			memcpy(buffer_, other.getBuffer(), other.getLength()+1);
			length_ = other.getLength();
		}
		DataBuffer(const char* buffer, unsigned int length) : buffer_(NULL) {
			buffer_ = new char[length+2];
			memcpy(buffer_, buffer, length+1);
			length_ = length;
		}
		DataBuffer(const unsigned char* buffer, unsigned int length) : buffer_(NULL) {
			buffer_ = new char[length+2];
			memcpy(buffer_, buffer, length+1);
			length_ = length;
		}
		virtual ~DataBuffer() {
			delete [] buffer_;
			length_ = 0;
			buffer_ = NULL;
		}
		void append(const char* buffer, const unsigned int length) {
			char *old = buffer_;
			char *tBuf = new char[length_+length+2];
			memcpy(tBuf, buffer_, length_);
			memcpy(&tBuf[length_], buffer, length);
			buffer_ = tBuf;
			delete [] old;
			length_ += length;
			buffer_[length_] = 0;
		}
		/**
		 * returns a const reference to the internal buffer
		 * Use with care!
		 *
		 * @access public 
		 * @returns const char *
		 * @qualifier const
		 */
		const char * getBuffer() const {
			return buffer_;
		}
		unsigned int getLength() const {
			return length_;
		}
		/**
		 * Eats a specified number of bytes from the beginning of the buffer
		 * @access public 
		 * @returns void
		 * @qualifier
		 * @param const unsigned int length the amount of bytes to eat
		 */
		void nibble(const unsigned int length) {
			if (length > length_)
				return;
			char *old = buffer_;
			unsigned int newLen = length_-length;
			char *tBuf = new char[newLen+2];
			memcpy(tBuf, &buffer_[length], newLen+1);
			buffer_ = tBuf;
			length_ = newLen;
			delete [] old;
		}

		DataBuffer unshift(const unsigned int length) {
			DataBuffer ret;
			if (length > length_)
				return ret;
			ret.copyFrom(buffer_, length);
			unsigned int newLen = length_-length;
			char *tBuf = new char[newLen+2];
			memcpy(tBuf, &buffer_[length], newLen+1);
			char *old = buffer_;
			buffer_ = tBuf;
			length_ = newLen;
			delete [] old;
			return ret;
		}
		const unsigned long long find(char c) {
			if (buffer_ == NULL)
				return -1;
			if (length_ == 0)
				return -1;
			const char *pos = strchr(buffer_, c);
			if (pos == NULL)
				return -1;
			return pos-buffer_;
		}
		void copyFrom(const char* buffer, const unsigned int length) {
			char *old = buffer_;
			buffer_ = new char[length+2];
			memcpy(buffer_, buffer, length);
			length_ = length;
			buffer_[length_] = 0;
			delete [] old;
		}
		std::string toString() {
			return strEx::format_buffer(buffer_, length_);
		}
	};

	class Socket {
	protected:
		SOCKET socket_;
		sockaddr_in from_;
		sockaddr_in to_;
		fd_set read_, write_, excp_;

	public:
		static std::wstring getLastError() {
			return error::format::from_system(::WSAGetLastError());
		}
		Socket() : socket_(NULL) {
			FD_ZERO(&read_);
			FD_ZERO(&write_);
			FD_ZERO(&excp_);
		}
		Socket(SOCKET socket) : socket_(socket) {
			FD_ZERO(&read_);
			FD_ZERO(&write_);
			FD_ZERO(&excp_);
		}
		Socket(int type, int protocol) : socket_(NULL) {
			socket_ = ::socket(AF_INET, type, protocol);
			FD_ZERO(&read_);
			FD_ZERO(&write_);
			FD_ZERO(&excp_);
		}
		Socket(bool create) : socket_(NULL) {
			if (create)
				socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			FD_ZERO(&read_);
			FD_ZERO(&write_);
			FD_ZERO(&excp_);
		}

		Socket(Socket &other) {
			socket_ = other.socket_;
			from_ = other.from_;
			other.socket_ = NULL;
			read_ = other.read_;
			write_ = other.write_;
			excp_ = other.excp_;
		}
		virtual ~Socket() {
			if (socket_)
				closesocket(socket_);
			socket_ = NULL;
		}
		virtual SOCKET detach() {
			SOCKET s = socket_;
			socket_ = NULL;
			return s;
		}
		virtual void attach(SOCKET s) {
			if (socket_ != NULL)
				throw SocketException(_T("Cant attach to existing socket!"));
			socket_ = s;
		}
		virtual void shutdown(int how = SD_BOTH) {
			if (socket_)
				::shutdown(socket_, how);
		}
		virtual int connect(std::wstring host, u_short port) {
			if (socket_) {
				to_.sin_family = AF_INET;
				to_.sin_port = htons(port);
				to_.sin_addr.s_addr = inet_addr(host);
				return connect_();
			}
			return SOCKET_ERROR;
		}
		virtual int connect_() {
			return ::connect(socket_, (SOCKADDR*) &to_, sizeof(to_));
		}

		virtual void close() {
			if (socket_)
				::closesocket(socket_);
			socket_ = NULL;
		}
		virtual void setLinger(int timeout) {
			if (this->setsockopt(SOL_SOCKET, SO_LINGER, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
				throw SocketException(_T("Failed to set linger on socket: "), ::WSAGetLastError());
		}
		virtual int setsockopt(int level, int optname, const char * optval, int optlen) {
			return ::setsockopt(socket_, level, optname, optval, optlen);
		}
		virtual void setNonBlock() {
			unsigned long NoBlock = 1;
			FD_SET(socket_, &read_);
			FD_SET(socket_, &write_);
			FD_SET(socket_, &excp_);
			this->ioctlsocket(FIONBIO, &NoBlock);
		}
		virtual void setBlock() {
			unsigned long Block = 0;
			FD_SET(socket_, &read_);
			FD_SET(socket_, &write_);
			FD_SET(socket_, &excp_);
			this->ioctlsocket(FIONBIO, &Block);
		}
		virtual bool canRead(long timeout = -1) {
			timeval timeout_;
			timeout_.tv_sec = timeout;
			FD_SET(socket_, &read_);
			FD_SET(socket_, &write_);
			FD_SET(socket_, &excp_);
			if (timeout == -1)
				::select(NULL, &read_, &write_, &excp_, NULL);
			else
				::select(NULL, &read_, &write_, &excp_, &timeout_);
			if (FD_ISSET(socket_, &read_))
				return true;
			return false;
		}
		virtual bool canWrite(long timeout = -1) {
			timeval timeout_;
			timeout_.tv_sec = timeout;
			FD_SET(socket_, &read_);
			FD_SET(socket_, &write_);
			FD_SET(socket_, &excp_);
			if (timeout == -1)
				::select(NULL, &read_, &write_, &excp_, NULL);
			else
				::select(NULL, &read_, &write_, &excp_, &timeout_);
			if (FD_ISSET(socket_, &write_))
				return true;
			return false;
		}

		static unsigned long inet_addr(std::wstring addr) {
			return ::inet_addr(strEx::wstring_to_string(addr).c_str());
		}
		static std::wstring inet_ntoa(unsigned long addr) {
			struct in_addr a;
			a.S_un.S_addr = addr;
			return strEx::string_to_wstring(::inet_ntoa(a));
		}
		static std::wstring getHostByName(std::wstring ip) {
			hostent* remoteHost;
			remoteHost = gethostbyname(strEx::wstring_to_string(ip).c_str());
			if (remoteHost == NULL)
				throw SocketException(_T("gethostbyname failed for ") + ip + _T(": "), ::WSAGetLastError());
			// @todo investigate it this is "correct" and dont use before!
			return strEx::string_to_wstring(::inet_ntoa(*reinterpret_cast<in_addr*>(remoteHost->h_addr)));
		}
		static struct in_addr getHostByNameAsIN(std::wstring ip) {
			hostent* remoteHost;
			remoteHost = gethostbyname(strEx::wstring_to_string(ip).c_str());
			if (remoteHost == NULL)
				throw SocketException(_T("gethostbyname failed for ") + ip + _T(": "), ::WSAGetLastError());
			if (remoteHost->h_addrtype != AF_INET) {
				throw SocketException(_T("gethostbyname failed for ") + ip + _T(": "), ::WSAGetLastError());
			}
			struct in_addr ret;
			ret.S_un.S_addr = (reinterpret_cast<in_addr*>(remoteHost->h_addr_list[0]))->S_un.S_addr;
			return ret;
		}
		static std::wstring getHostByAddr(std::wstring ip) {
			hostent* remoteHost;
			remoteHost = gethostbyaddr(strEx::wstring_to_string(ip).c_str(), static_cast<int>(ip.length()), AF_INET);
			if (remoteHost == NULL)
				throw SocketException(_T("gethostbyaddr failed for ") + ip + _T(": "), ::WSAGetLastError());
			return strEx::string_to_wstring(remoteHost->h_name);
		}
		static struct in_addr getHostByAddrAsIN(std::wstring ip) {
			hostent* remoteHost;
			unsigned int addr = ::inet_addr(strEx::wstring_to_string(ip).c_str());
			std::cerr << "addr: " << addr << std::endl;
			remoteHost = ::gethostbyaddr(reinterpret_cast<char*>(&addr), 4, AF_INET);
			if (remoteHost == NULL)
				throw SocketException(_T("gethostbyaddr failed for ") + ip + _T(": "), ::WSAGetLastError());
			if (remoteHost->h_addrtype != AF_INET)
				throw SocketException(_T("gethostbyname returned the wrong type ") + ip + _T(": "), ::WSAGetLastError());
			struct in_addr ret;
			ret.S_un.S_addr = (reinterpret_cast<in_addr*>(remoteHost->h_addr_list[0]))->S_un.S_addr;
			return ret;
		}
		virtual bool readAll(DataBuffer &buffer, unsigned int tmpBufferLength = 1024, int maxLength = -1);
		virtual bool sendAll(const char * buffer, unsigned int len);

		bool inline sendAll(DataBuffer &buffer) {
			return sendAll(buffer.getBuffer(), buffer.getLength());
		}

		virtual int send(const char * buf, unsigned int len, int flags = 0) {
			if (!socket_)
				throw SocketException(_T("send:: cant send to uninitialized socket"));
			return ::send(socket_, buf, len, flags);
		}
		int inline send(DataBuffer &buffer, int flags = 0) {
			return send(buffer.getBuffer(), buffer.getLength(), flags);
		}

		virtual void socket(int af, int type, int protocol ) {
			socket_ = ::socket(af, type, protocol);
			if (socket_ == INVALID_SOCKET)
				throw SocketException(_T("Failed to create socket"), WSAGetLastError());
		}
		virtual void bind() {
			if (!socket_)
				throw SocketException(_T("bind:: Invalid socket"));
			int fromlen=sizeof(from_);
			if (::bind(socket_, (sockaddr*)&from_, fromlen) == SOCKET_ERROR)
				throw SocketException(_T("bind failed: "), ::WSAGetLastError());
		}
		virtual void listen(int backlog = SOMAXCONN) {
			if (!socket_)
				throw SocketException(_T("listen:: Invalid socket"));
			if (::listen(socket_, backlog) == SOCKET_ERROR)
				throw SocketException(_T("listen failed: "), ::WSAGetLastError());
		}
		virtual bool accept(Socket &client) {
			int fromlen=sizeof(client.from_);
			SOCKET s = ::accept(socket_, (sockaddr*)&client.from_, &fromlen);
			if(s == INVALID_SOCKET) {
				int err = ::WSAGetLastError();
				if (err == WSAEWOULDBLOCK)
					return false;
				throw SocketException(_T("accept failed: "), ::WSAGetLastError());
			}
			client.attach(s);
			return true;
		}
		virtual void setAddr(short family, u_long addr, u_short port) {
			from_.sin_family=family;
			from_.sin_addr.s_addr=addr;
			from_.sin_port=port;
		}
		virtual void ioctlsocket(long cmd, u_long *argp) {
			if (!socket_)
				throw SocketException(_T("ioctlsocket:: Invalid socket"));
			if (::ioctlsocket(socket_, cmd, argp) == SOCKET_ERROR)
				throw SocketException(_T("ioctlsocket failed: "), ::WSAGetLastError());
		}
		virtual std::wstring getAddrString() {
			return strEx::string_to_wstring(::inet_ntoa(from_.sin_addr));
		}
		virtual struct in_addr getAddr() {
			return from_.sin_addr;
		}
		virtual void printError(std::wstring file, int line, std::wstring error);
	};

	class ListenerHandler {
	public:
		virtual void onAccept(Socket *client) = 0;
		virtual void onClose() = 0;
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
	template <class TListenerType = simpleSocket::Socket, class TSocketType = TListenerType>
	class Listener : public TListenerType {
	public:
		typedef TListenerType tListener;
		typedef TSocketType tSocket;
	private:
		struct simpleResponderBundle {
			bool terminated;
			HANDLE hThread;
			unsigned dwThreadID;
		};
		typedef std::list<simpleResponderBundle> socketResponses;
		typedef TListenerType tBase;
		class ListenerThread;
		typedef Thread<ListenerThread> listenThreadManager;

		u_short bindPort_;
		u_long bindAddres_;
		unsigned int listenQue_;
		listenThreadManager threadManager_;
		socketResponses responderList_;
		MutexHandler responderMutex_;

	public:
		class ListenerThread {
		private:
			typedef TListenerType tParentBase;
			typedef TSocketType tSocket;

			HANDLE hStopEvent_;
		public:
			ListenerThread() : hStopEvent_(NULL) {}
			DWORD threadProc(LPVOID lpParameter);
			bool hasThread() const {
				return hStopEvent_ != NULL;
			}
			void exitThread(void) {
				if (hStopEvent_ == NULL)
					throw SocketException(_T("exitThread:: no stop event??"));
				if (!SetEvent(hStopEvent_))
					throw SocketException(_T("SetEvent failed."));
			}
		};
	private:
		ListenerHandler *pHandler_;

	public:
		Listener() : pHandler_(NULL), bindPort_(0), bindAddres_(INADDR_ANY), listenQue_(0), threadManager_(_T("listenThreadManager")) {};
		virtual ~Listener() {
			if (responderList_.size() > 0) {
				MutexLock lock(responderMutex_);
				if (!lock.hasMutex()) {
					printError(_T(__FILE__), __LINE__, _T("Failed to get responder mutex (cannot terminate socket threads)."));
				} else {
					for (socketResponses::iterator it = responderList_.begin(); it != responderList_.end(); ++it) {
						if (WaitForSingleObject( (*it).hThread, 1000) == WAIT_OBJECT_0) {
						} else {
							if (!TerminateThread((*it).hThread, -1)) {
								printError(_T(__FILE__), __LINE__, _T("We failed to terminate check thread."));
							} else {
								if (WaitForSingleObject( (*it).hThread, 5000) == WAIT_OBJECT_0) {
									CloseHandle((*it).hThread);
								} else {
									printError(_T(__FILE__), __LINE__, _T("We failed to terminate check thread (wait timed out)."));
								}
							}
						}
					}
					responderList_.clear();
				}
			}
		};
/*
		virtual void StartListener(int port) {
			bindPort_ = port;
			threadManager_.createThread(this);
		}
		*/
		bool hasListener() {
			try {
				if (threadManager_.hasActiveThread()) {
					const ListenerThread *t = threadManager_.getThreadConst();
					if (t!=NULL)
						return t->hasThread();
				}
			} catch (ThreadException e) {
				printError(_T(__FILE__), __LINE__, _T("Could not access listener thread!"));
				return false;
			}
			return false;
		}
		virtual void StartListener(std::wstring host, int port, int queLength) {
			bindPort_ = port;
			if (!host.empty())
				bindAddres_ = TListenerType::inet_addr(host);
			if (bindAddres_ == INADDR_NONE)
				bindAddres_ = INADDR_ANY;
			listenQue_ = queLength;
			try {
				threadManager_.createThread(this);
			} catch (ThreadException e) {
				throw SocketException(_T("Could not start thread (got exception in thread): ") + e.e_);
			}
		}
		virtual void StopListener() {
			try {
				if (threadManager_.hasActiveThread())
					if (!threadManager_.exitThread()) {
						tBase::close();
						throw SocketException(_T("Could not terminate thread."));
					}
			} catch (ThreadException e) {
				tBase::close();
				throw SocketException(_T("Could not terminate thread (got exception in thread): ") + e.e_);
			}
			tBase::close();
		}
		void setHandler(ListenerHandler* pHandler) {
			pHandler_ = pHandler;
		}
		void removeHandler(ListenerHandler* pHandler) {
			if (pHandler != pHandler_)
				throw SocketException(_T("Not a registered handler!"));
			pHandler_ = NULL;
		}
		static unsigned __stdcall socketResponceProc(void* lpParameter);
		struct srp_data {
			Listener *pCore;
			tSocket *client;
		};
		void cleanResponders(){
			MutexLock lock(responderMutex_);
			if (lock.hasMutex()) {
				cleanResponders_RAW();
			}
		}
		private:
			void cleanResponders_RAW(){
				for (socketResponses::iterator it = responderList_.begin(); it != responderList_.end();) {
					if ( (*it).terminated) {
						if (WaitForSingleObject( (*it).hThread, 500) == WAIT_OBJECT_0) {
							//printError(_T(__FILE__), __LINE__, _T("Closing: ") + strEx::itos((*it).dwThreadID));
							CloseHandle((*it).hThread);
							responderList_.erase(it++);
						}
					} else
						++it;
				}
			}

		public:

		void addResponder(tSocket *client) {
			MutexLock lock(responderMutex_);
			if (!lock.hasMutex()) {
				client->close();
				delete client;
				printError(_T(__FILE__), __LINE__, _T("Failed to get responder mutex."));
				return;
			}
			cleanResponders_RAW();
			simpleResponderBundle data;
			srp_data *lpData = new srp_data;
			lpData->pCore = this;
			lpData->client = client;

			data.hThread = reinterpret_cast<HANDLE>(::_beginthreadex( NULL, 0, &socketResponceProc, lpData, 0, &data.dwThreadID));
			//printError(_T(__FILE__), __LINE__, _T("Adding responder.") + strEx::itos(data.dwThreadID));
			data.terminated = false;
			responderList_.push_back(data);

			/*
			delete lpData;
			client->close();
			delete client;
			*/
		}
		bool removeResponder(DWORD dwThreadID) {
			//printError(_T(__FILE__), __LINE__, _T("Terminating.") + strEx::itos(dwThreadID));
			MutexLock lock(responderMutex_);
			if (!lock.hasMutex()) {
				printError(_T(__FILE__), __LINE__, _T("Failed to get responder mutex when trying to free thread."));
				return false;
			}
			for (socketResponses::iterator it = responderList_.begin(); it != responderList_.end(); ++it) {
				if ( (*it).dwThreadID == dwThreadID) {
					(*it).terminated = true;
					return true;
				}
			}
			printError(_T(__FILE__), __LINE__, _T("Failed to remove socket-responder."));
			return false;
		}


	private:
		void onAccept(tSocket *client) {
			if (pHandler_)
				pHandler_->onAccept(client);
		}
		void onClose() {
			if (pHandler_)
				pHandler_->onClose();
		}
		virtual bool accept(tSocket &client) {
			return tBase::accept(client);
		}
	};

	WSADATA WSAStartup(WORD wVersionRequested = 0x202);
	void WSACleanup();

}

template <class TListenerType, class TSocketType>
unsigned simpleSocket::Listener<TListenerType, TSocketType>::socketResponceProc(void* lpParameter)
{
	// @todo make sure this terminates after X seconds!

	srp_data *data = reinterpret_cast<srp_data*>(lpParameter);
	Listener *pCore = data->pCore;
	tSocket *client = data->client;
	delete data;
	try {
		pCore->onAccept(client);
	} catch (SocketException e) {
		pCore->printError(_T(__FILE__), __LINE__, e.getMessage() + _T(" killing socket..."));
	} catch (...) {
		pCore->printError(_T(__FILE__), __LINE__, _T("<UNHANDLED EXCEPTION> killing socket..."));
	}
	client->close();
	delete client;
	if (!pCore->removeResponder(GetCurrentThreadId())) {
		pCore->printError(_T(__FILE__), __LINE__, _T("Could not remove thread: ") + strEx::itos(GetCurrentThreadId()));
	}
	//_endthreadex(0);
	return 0;
}


template <class TListenerType, class TSocketType>
DWORD simpleSocket::Listener<TListenerType, TSocketType>::ListenerThread::threadProc(LPVOID lpParameter)
{
	Listener *core = reinterpret_cast<Listener*>(lpParameter);

	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		core->printError(_T(__FILE__), __LINE__, _T("Create StopEvent failed: ") + error::lookup::last_error());
		return 0;
	}

	bool socketOk = false;
	try {
		core->socket(AF_INET,SOCK_STREAM,0);
		core->setAddr(AF_INET, core->bindAddres_, htons(core->bindPort_));
		core->bind();
		NSC_DEBUG_MSG_STD(_T("Bound to: ") + TListenerType::inet_ntoa(core->bindAddres_) + _T(":") + strEx::itos(core->bindPort_));
		if (core->listenQue_ != 0)
			core->listen(core->listenQue_);
		else
			core->listen();
		core->setNonBlock();
		socketOk = true;
	} catch (SocketException e) {
		core->printError(_T(__FILE__), __LINE__, e.getMessage());
	} catch (...) {
		core->printError(_T(__FILE__), __LINE__, _T("Unhandeled exception in the socket thread..."));
	}
#define CLEAN_INTERVAL 10
	if (socketOk) {
		try {
			//NSC_DEBUG_MSG_STD("Socket ready...");
			int count =0;
			while (!(WaitForSingleObject(hStopEvent_, 100) == WAIT_OBJECT_0)) {
				try {
					tSocket client;
					if (core->accept(client)) {
						core->addResponder(new tSocket(client));
						count = 0;
					} else if (count > CLEAN_INTERVAL) {
						core->cleanResponders();
						count = 0;
					} else {
						count++;
					}
				} catch (SocketException e) {
					core->printError(_T(__FILE__), __LINE__, e.getMessage() + _T(", attempting to resume..."));
				}
			}
		} catch (SocketException e) {
			core->printError(_T(__FILE__), __LINE__, e.getMessage());
		} catch (...) {
			core->printError(_T(__FILE__), __LINE__, _T("Unhandeled exception in the socket thread..."));
		}
	} else {
		core->printError(_T(__FILE__), __LINE__, _T("Socket did not start properly, we will now do nothing..."));
		WaitForSingleObject(hStopEvent_, INFINITE);
	}
	NSC_DEBUG_MSG_STD(_T("Socket listener is preparing to shutdown..."));
	core->shutdown(SD_BOTH);
	core->close();
	core->onClose();
	HANDLE hTmp = hStopEvent_;
	hStopEvent_ = NULL;
	if (!CloseHandle(hTmp)) {
		core->printError(_T(__FILE__), __LINE__, _T("CloseHandle StopEvent failed: ") + error::lookup::last_error());
	}
	return 0;
}



namespace socketHelpers {
	class allowedHosts {
		struct host_record {
			host_record() : mask(0) {}
			host_record(std::wstring r) : mask(0), record(r) {}
			std::wstring record;
			std::wstring host;
			u_long in_addr;
			unsigned long mask;
		};
	public:
		typedef std::list<host_record> host_list; 
	private:
		host_list allowedHosts_;
		bool cachedAddresses_;
	public:
		allowedHosts() : cachedAddresses_(true) {}

		unsigned int lookupMask(std::wstring mask) {
			unsigned int masklen = 32;
			if (!mask.empty()) {
				std::wstring::size_type pos = mask.find_first_of(_T("0123456789"));
				if (pos != std::wstring::npos) {
					masklen = strEx::stoi(mask.substr(pos));
				}
			}
			if (masklen > 32)
				masklen = 32;
			return ::ntohl((0xffffffff >> (32 - masklen )) << (32 - masklen));
		}
		void lookupList() {
			for (host_list::iterator it = allowedHosts_.begin();it!=allowedHosts_.end();++it) {
				std::wstring record = (*it).record;
				if (record.length() > 0) {
					try {
						std::wstring::size_type pos = record.find('/');
						if (pos == std::wstring::npos) {
							(*it).host = record;
							(*it).mask = lookupMask(_T(""));
						} else {
							(*it).host = record.substr(0, pos);
							(*it).mask = lookupMask(record.substr(pos));
						}
						if ((!(*it).host.empty()) && isalpha((*it).host[0]))
							(*it).in_addr = simpleSocket::Socket::getHostByNameAsIN((*it).host).S_un.S_addr;
						else
							(*it).in_addr = ::inet_addr(strEx::wstring_to_string((*it).host).c_str()); // simpleSocket::Socket::getHostByAddrAsIN((*it).host);
						/*
						std::wcerr << _T("Added: ")
							<< simpleSocket::Socket::inet_ntoa((*it).in_addr)
							<< _T(" with mask ")
							<< simpleSocket::Socket::inet_ntoa((*it).mask)
							//<< _T("(") << (*it).mask << _T(")") 
							<< _T(" from ")
							<< (*it).record <<
							std::endl;
							*/
							
							
					} catch (simpleSocket::SocketException e) {
						std::wcerr << _T("Filed to lookup host: ") << e.getMessage() << std::endl;
					}
				}
			}
		}

		void setAllowedHosts(const std::list<std::wstring> list, bool cachedAddresses) {
			for (std::list<std::wstring>::const_iterator it = list.begin(); it != list.end(); ++it) {
				allowedHosts_.push_back(host_record(strEx::trim(*it, _T(" \t"))));
			}
			cachedAddresses_ = cachedAddresses;
//			if ((!allowedHosts.empty()) && (allowedHosts.front() == "") )
//				allowedHosts.pop_front();
			//allowedHosts_ = allowedHosts;
			lookupList();
		}
		bool matchHost(host_record allowed, struct in_addr remote) {
			/*
			if ((allowed.in_addr&allowed.mask)==(remote.S_un.S_addr&allowed.mask)) {
				std::cerr << "Matched: " << simpleSocket::Socket::inet_ntoa(allowed.in_addr)  << " with " << 
					simpleSocket::Socket::inet_ntoa(remote.S_un.S_addr) << std::endl;
			}
			*/
			return ((allowed.in_addr&allowed.mask)==(remote.S_un.S_addr&allowed.mask));
		}
		bool inAllowedHosts(struct in_addr remote) {
			if (allowedHosts_.empty())
				return true;
			host_list::const_iterator cit;
			if (!cachedAddresses_) {
				lookupList();
			}
			for (cit = allowedHosts_.begin();cit!=allowedHosts_.end();++cit) {
				if (matchHost((*cit), remote))
					return true;
			}
			return false;
		}
	};
}