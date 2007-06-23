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
			buffer_ = new char[other.getLength()+2];
			memcpy(buffer_, other.getBuffer(), other.getLength()+1);
			length_ = other.getLength();
		}
		virtual ~DataBuffer() {
			delete [] buffer_;
			length_ = 0;
			buffer_ = NULL;
		}
		void append(const char* buffer, const unsigned int length) {
			char *tBuf = new char[length_+length+2];
			memcpy(tBuf, buffer_, length_);
			memcpy(&tBuf[length_], buffer, length);
			delete [] buffer_;
			buffer_ = tBuf;
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
			unsigned int newLen = length_-length;
			char *tBuf = new char[newLen+2];
			memcpy(tBuf, &buffer_[length], newLen+1);
			char *oldBuf = buffer_;
			buffer_ = tBuf;
			length_ = newLen;
			delete [] oldBuf;
		}

		DataBuffer unshift(const unsigned int length) {
			DataBuffer ret;
			if (length > length_)
				return ret;
			ret.copyFrom(buffer_, length);
			unsigned int newLen = length_-length;
			char *tBuf = new char[newLen+2];
			memcpy(tBuf, &buffer_[length], newLen+1);
			char *oldBuf = buffer_;
			buffer_ = tBuf;
			length_ = newLen;
			delete [] oldBuf;
			return ret;
		}
		const unsigned long long find(char c) {
			if (buffer_ == NULL)
				return 0;
			if (length_ == 0)
				return 0;
			const char *pos = strchr(buffer_, c);
			if (pos == NULL)
				return 0;
			return pos-buffer_;
		}
		void copyFrom(const char* buffer, const unsigned int length) {
			delete [] buffer_;
			buffer_ = new char[length+2];
			memcpy(buffer_, buffer, length);
			length_ = length;
			buffer_[length_] = 0;
		}
	};

	class Socket {
	protected:
		SOCKET socket_;
		sockaddr_in from_;

	public:
		Socket() : socket_(NULL) {
		}
		Socket(SOCKET socket) : socket_(socket) {
		}
		Socket(Socket &other) {
			socket_ = other.socket_;
			from_ = other.from_;
			other.socket_ = NULL;
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
			assert(socket_ == NULL);
			socket_ = s;
		}
		virtual void shutdown(int how = SD_BOTH) {
			if (socket_)
				::shutdown(socket_, how);
		}

		virtual void close() {
			if (socket_)
				closesocket(socket_);
			socket_ = NULL;
		}
		virtual void setNonBlock() {
			unsigned long NoBlock = 1;
			this->ioctlsocket(FIONBIO, &NoBlock);
		}
		static unsigned long inet_addr(std::string addr) {
			return ::inet_addr(addr.c_str());
		}
		static std::string inet_ntoa(unsigned long addr) {
			struct in_addr a;
			a.S_un.S_addr = addr;
			return ::inet_ntoa(a);
		}
		static std::string getHostByName(std::string ip) {
			hostent* remoteHost;
			remoteHost = gethostbyname(ip.c_str());
			if (remoteHost == NULL)
				throw SocketException("gethostbyname failed for " + ip + ": ", ::WSAGetLastError());
			// @todo investigate it this is "correct" and dont use before!
			return ::inet_ntoa(*reinterpret_cast<in_addr*>(remoteHost->h_addr));
		}
		static struct in_addr getHostByNameAsIN(std::string ip) {
			hostent* remoteHost;
			remoteHost = gethostbyname(ip.c_str());
			if (remoteHost == NULL)
				throw SocketException("gethostbyname failed for " + ip + ": ", ::WSAGetLastError());
			if (remoteHost->h_addrtype != AF_INET) {
				throw SocketException("gethostbyname failed for " + ip + ": ", ::WSAGetLastError());
			}
			struct in_addr ret;
			ret.S_un.S_addr = (reinterpret_cast<in_addr*>(remoteHost->h_addr_list[0]))->S_un.S_addr;
			return ret;
		}
		static std::string getHostByAddr(std::string ip) {
			hostent* remoteHost;
			remoteHost = gethostbyaddr(ip.c_str(), static_cast<int>(ip.length()), AF_INET);
			if (remoteHost == NULL)
				throw SocketException("gethostbyaddr failed for " + ip + ": ", ::WSAGetLastError());
			return remoteHost->h_name;
		}
		static struct in_addr getHostByAddrAsIN(std::string ip) {
			hostent* remoteHost;
			unsigned int addr = ::inet_addr(ip.c_str());
			std::cerr << "addr: " << addr << std::endl;
			remoteHost = ::gethostbyaddr(reinterpret_cast<char*>(&addr), 4, AF_INET);
			if (remoteHost == NULL)
				throw SocketException("gethostbyaddr failed for " + ip + ": ", ::WSAGetLastError());
			if (remoteHost->h_addrtype != AF_INET)
				throw SocketException("gethostbyname returned the wrong type " + ip + ": ", ::WSAGetLastError());
			struct in_addr ret;
			ret.S_un.S_addr = (reinterpret_cast<in_addr*>(remoteHost->h_addr_list[0]))->S_un.S_addr;
			return ret;
		}
		virtual void readAll(DataBuffer &buffer, unsigned int tmpBufferLength = 1024);

		virtual void socket(int af, int type, int protocol ) {
			socket_ = ::socket(af, type, protocol);
			assert(socket_ != INVALID_SOCKET);
		}
		virtual void bind() {
			assert(socket_);
			int fromlen=sizeof(from_);
			if (::bind(socket_, (sockaddr*)&from_, fromlen) == SOCKET_ERROR)
				throw SocketException("bind failed: ", ::WSAGetLastError());
		}
		virtual void listen(int backlog = SOMAXCONN) {
			assert(socket_);
			if (::listen(socket_, backlog) == SOCKET_ERROR)
				throw SocketException("listen failed: ", ::WSAGetLastError());
		}
		virtual bool accept(Socket &client) {
			int fromlen=sizeof(client.from_);
			SOCKET s = ::accept(socket_, (sockaddr*)&client.from_, &fromlen);
			if(s == INVALID_SOCKET) {
				int err = ::WSAGetLastError();
				if (err == WSAEWOULDBLOCK)
					return false;
				throw SocketException("accept failed: ", ::WSAGetLastError());
			}
			client.attach(s);
			return true;
		}
		virtual void setAddr(short family, u_long addr, u_short port) {
			from_.sin_family=family;
			from_.sin_addr.s_addr=addr;
			from_.sin_port=port;
		}
		virtual int send(const char * buf, unsigned int len, int flags = 0) {
			assert(socket_);
			return ::send(socket_, buf, len, flags);
		}
		int inline send(DataBuffer &buffer, int flags = 0) {
			return send(buffer.getBuffer(), buffer.getLength(), flags);
		}
		virtual void ioctlsocket(long cmd, u_long *argp) {
			assert(socket_);
			if (::ioctlsocket(socket_, cmd, argp) == SOCKET_ERROR)
				throw SocketException("ioctlsocket failed: ", ::WSAGetLastError());
		}
		virtual std::string getAddrString() {
			return ::inet_ntoa(from_.sin_addr);
		}
		virtual struct in_addr getAddr() {
			return from_.sin_addr;
		}
		virtual void printError(std::string file, int line, std::string error);
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
				assert(hStopEvent_ != NULL);
				if (!SetEvent(hStopEvent_))
					throw new SocketException("SetEvent failed.");
			}
		};
	private:
		ListenerHandler *pHandler_;

	public:
		Listener() : pHandler_(NULL), bindPort_(0), bindAddres_(INADDR_ANY), listenQue_(0), threadManager_("listenThreadManager") {};
		virtual ~Listener() {
			if (responderList_.size() > 0) {
				MutexLock lock(responderMutex_);
				if (!lock.hasMutex()) {
					printError(__FILE__, __LINE__, "Failed to get responder mutex (cannot terminate socket threads).");
				} else {
					for (socketResponses::iterator it = responderList_.begin(); it != responderList_.end(); ++it) {
						if (WaitForSingleObject( (*it).hThread, 1000) == WAIT_OBJECT_0) {
						} else {
							if (!TerminateThread((*it).hThread, -1)) {
								printError(__FILE__, __LINE__, "We failed to terminate check thread.");
							} else {
								if (WaitForSingleObject( (*it).hThread, 5000) == WAIT_OBJECT_0) {
									CloseHandle((*it).hThread);
								} else {
									printError(__FILE__, __LINE__, "We failed to terminate check thread (wait timed out).");
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
				printError(__FILE__, __LINE__, "Could not access listener thread!");
				return false;
			}
			return false;
		}
		virtual void StartListener(std::string host, int port, int queLength) {
			bindPort_ = port;
			if (!host.empty())
				bindAddres_ = TListenerType::inet_addr(host);
			if (bindAddres_ == INADDR_NONE)
				bindAddres_ = INADDR_ANY;
			listenQue_ = queLength;
			threadManager_.createThread(this);
		}
		virtual void StopListener() {
			try {
				if (threadManager_.hasActiveThread())
					if (!threadManager_.exitThread()) {
						tBase::close();
						throw new SocketException("Could not terminate thread.");
					}
			} catch (ThreadException e) {
				tBase::close();
				throw new SocketException("Could not terminate thread (got exception in thread).");
			}
			tBase::close();
		}
		void setHandler(ListenerHandler* pHandler) {
			pHandler_ = pHandler;
		}
		void removeHandler(ListenerHandler* pHandler) {
			if (pHandler != pHandler_)
				throw SocketException("Not a registered handler!");
			pHandler_ = NULL;
		}
		static unsigned __stdcall socketResponceProc(void* lpParameter);
		struct srp_data {
			Listener *pCore;
			tSocket *client;
		};
		void addResponder(tSocket *client) {
			MutexLock lock(responderMutex_);
			if (!lock.hasMutex()) {
				printError(__FILE__, __LINE__, "Failed to get responder mutex.");
				return;
			}
			for (socketResponses::iterator it = responderList_.begin(); it != responderList_.end();) {
				if ( (*it).terminated) {
					if (WaitForSingleObject( (*it).hThread, 500) == WAIT_OBJECT_0) {
						CloseHandle((*it).hThread);
						responderList_.erase(it++);
					}
				} else
					++it;
			}
			simpleResponderBundle data;
			srp_data *lpData = new srp_data;
			lpData->pCore = this;
			lpData->client = client;

			data.hThread = reinterpret_cast<HANDLE>(::_beginthreadex( NULL, 0, &socketResponceProc, lpData, 0, &data.dwThreadID));
			data.terminated = false;
			responderList_.push_back(data);
		}
		bool removeResponder(DWORD dwThreadID) {
			MutexLock lock(responderMutex_);
			if (!lock.hasMutex()) {
				printError(__FILE__, __LINE__, "Failed to get responder mutex when trying to free thread.");
				return false;
			}
			for (socketResponses::iterator it = responderList_.begin(); it != responderList_.end(); ++it) {
				if ( (*it).dwThreadID == dwThreadID) {
					(*it).terminated = true;
					return true;
				}
			}
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
		pCore->printError(__FILE__, __LINE__, e.getMessage() + " killing socket...");
	}
	client->close();
	delete client;
	if (!pCore->removeResponder(GetCurrentThreadId())) {
		pCore->printError(__FILE__, __LINE__, "Could not remove thread: " + strEx::itos(GetCurrentThreadId()));
	}
	_endthreadex(0);
	return 0;
}


template <class TListenerType, class TSocketType>
DWORD simpleSocket::Listener<TListenerType, TSocketType>::ListenerThread::threadProc(LPVOID lpParameter)
{
	Listener *core = reinterpret_cast<Listener*>(lpParameter);

	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		core->printError(__FILE__, __LINE__, "Create StopEvent failed: " + strEx::itos(GetLastError()));
		return 0;
	}

	try {
		core->socket(AF_INET,SOCK_STREAM,0);
		core->setAddr(AF_INET, core->bindAddres_, htons(core->bindPort_));
		core->bind();
		NSC_DEBUG_MSG_STD("Bound to: " + TListenerType::inet_ntoa(core->bindAddres_) + ":" + strEx::itos(core->bindPort_));
		if (core->listenQue_ != 0)
			core->listen(core->listenQue_);
		else
			core->listen();
		core->setNonBlock();
		NSC_DEBUG_MSG_STD("Socket ready...");
		while (!(WaitForSingleObject(hStopEvent_, 100) == WAIT_OBJECT_0)) {
			try {
				tSocket client;
				if (core->accept(client)) {
					core->addResponder(new tSocket(client));
				}
			} catch (SocketException e) {
				core->printError(__FILE__, __LINE__, e.getMessage() + ", attempting to resume...");
			}
		}
	} catch (SocketException e) {
		core->printError(__FILE__, __LINE__, e.getMessage());
	}
	NSC_DEBUG_MSG_STD("Listener is preparing to shutdown...");
	core->shutdown(SD_BOTH);
	core->close();
	core->onClose();
	HANDLE hTmp = hStopEvent_;
	hStopEvent_ = NULL;
	if (!CloseHandle(hTmp)) {
		core->printError(__FILE__, __LINE__, "CloseHandle StopEvent failed: " + strEx::itos(GetLastError()));
	}
	return 0;
}



namespace socketHelpers {
	class allowedHosts {
		struct host_record {
			host_record() : mask(0) {}
			host_record(std::string r) : mask(0), record(r) {}
			std::string record;
			std::string host;
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

		unsigned int lookupMask(std::string mask) {
			unsigned int masklen = 32;
			if (!mask.empty()) {
				std::string::size_type pos = mask.find_first_of("0123456789");
				if (pos != std::string::npos) {
					masklen = strEx::stoi(mask.substr(pos));
				}
			}
			if (masklen > 32)
				masklen = 32;
			return (~((unsigned int)0))>>(32-masklen);
		}
		void lookupList() {
			for (host_list::iterator it = allowedHosts_.begin();it!=allowedHosts_.end();++it) {
				std::string record = (*it).record;
				if (record.length() > 0) {
					try {
						std::string::size_type pos = record.find('/');
						if (pos == std::string::npos) {
							(*it).host = record;
							(*it).mask = lookupMask("");
						} else {
							(*it).host = record.substr(0, pos);
							(*it).mask = lookupMask(record.substr(pos));
						}
						if (isalpha((*it).host[0]))
							(*it).in_addr = simpleSocket::Socket::getHostByNameAsIN((*it).host).S_un.S_addr;
						else
							(*it).in_addr = ::inet_addr((*it).host.c_str()); // simpleSocket::Socket::getHostByAddrAsIN((*it).host);
						/*
						std::cerr << "Added: " 
							+ simpleSocket::Socket::inet_ntoa((*it).in_addr)
							+ " with mask "
							+ simpleSocket::Socket::inet_ntoa((*it).mask)
							+ " from "
							+ (*it).record <<
							std::endl;
							*/
					} catch (simpleSocket::SocketException e) {
						std::cerr << "Filed to lokup host: " << e.getMessage() << std::endl;
					}
				}
			}
		}

		void setAllowedHosts(const std::list<std::string> list, bool cachedAddresses) {
			for (std::list<std::string>::const_iterator it = list.begin(); it != list.end(); ++it) {
				allowedHosts_.push_back(host_record(*it));
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