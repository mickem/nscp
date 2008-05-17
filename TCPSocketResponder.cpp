#include "stdafx.h"
#include "strEx.h"
#include "NSParser.h"
#include "NSClient++.h"
#include "TCPSocketResponder.h"
#include "Settings.h"

extern NSClient mainClient;

/**
 * Default c-tor
 */
TCPSocketResponder::TCPSocketResponder() : running_(false) {
}
/**
 * Return true if the socket is "running".
 * @return true if the socket is spoused to be running
 */
bool TCPSocketResponder::isRunning(void) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		LOG_ERROR_STD("Failed to get Mutex: " + strEx::itos(mutex.getWaitResult()));
		return false;
	}
	return running_;
}
/**
 * Set the running flag to "stop" (will prevent the socket to start waiting again)
 */
void TCPSocketResponder::stopRunning(void) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		LOG_ERROR_STD("Failed to get Mutex!" + strEx::itos(mutex.getWaitResult()));
		return;
	}
	running_ = false;
}
/**
* Set the running flag to "start" (will make the socket wait again)
 */
void TCPSocketResponder::startRunning(void) {
	MutexLock mutex(mutexHandler);
	if (!mutex.hasMutex()) {
		LOG_ERROR_STD("Failed to get Mutex!" + strEx::itos(mutex.getWaitResult()));
		return;
	}
	running_ = true;
}
/**
 * Parse a command string into command, password and arguments. Then the command will be executed bu the core.
 * @param request A request string to parse
 * @return The result (if any) of the executed command.
 */
std::string TCPSocketResponder::parseCommand(char* request) {
	std::list<std::string> args = charEx::split(request, '&');
	if (args.size() < 2) {
		return "Insufficient arguments!";
	}
	std::string password = args.front(); args.pop_front();
	std::string command = args.front(); args.pop_front();
	return mainClient.execute(password, command, args);
}
/**
 * Thread procedure for the socket listener
 * @param lpParameter Potential argument to the thread proc.
 * @return thread exit status
 * @todo This needs to be reworked, possibly completely redone ?
 */
DWORD TCPSocketResponder::threadProc(LPVOID lpParameter)
{
	startRunning();
	WSADATA wsaData;
	sockaddr_in local;
	int wsaret=WSAStartup(0x101,&wsaData);
	if(wsaret!=0) {
		LOG_ERROR_STD("WSA Startup failed: " + strEx::itos(wsaret));
		return 0;
	}

	local.sin_family=AF_INET;
	local.sin_addr.s_addr=INADDR_ANY;
	local.sin_port=htons(static_cast<u_short>(Settings::getInstance()->getInt("generic", "port", DEFAULT_TCP_PORT)));
	server=socket(AF_INET,SOCK_STREAM,0);
	if(server==INVALID_SOCKET) {
		LOG_ERROR_STD("Could not create listening socket: " + strEx::itos(GetLastError()));
		return 0;
	}

	if(bind(server,(sockaddr*)&local,sizeof(local))!=0) {
		LOG_ERROR_STD("Could not bind socket: " + strEx::itos(GetLastError()));
		return 0;
	}

	if(listen(server,10)!=0) {
		LOG_ERROR_STD("Could not open socket: " + strEx::itos(GetLastError()));
		return 0;
	}

	SOCKET client;
	sockaddr_in from;
	int fromlen=sizeof(from);

	while(isRunning()) {
		client=accept(server, (struct sockaddr*)&from,&fromlen);
		if (client != INVALID_SOCKET) {
			char *buff = new char[RECV_BUFFER_LEN+1];
			int n=recv(client,buff,RECV_BUFFER_LEN,0);
			if ((n!=SOCKET_ERROR )&&(n > 0)&&(n < RECV_BUFFER_LEN)) {
				buff[n] = '\0';
				LOG_DEBUG("Incoming data: ");
				LOG_DEBUG(buff);
				std::string ret = parseCommand(buff);
				LOG_DEBUG("Outgoing data: ");
				LOG_DEBUG(ret.c_str());
				send(client, ret.c_str(), ret.length(), 0);
			} else {
				std::string str = "ERROR: Unknown socket error";
				send(client,str.c_str(),str.length(),0);
			}
			delete [] buff;
			closesocket(client);
		}
	}
	closesocket(server);
	WSACleanup();
	LOG_DEBUG("Socket closed!");
	return 0;
}

/**
 * Exit thread callback proc. 
 * This is called by the thread manager when the thread should initiate a shutdown procedure.
 * The thread manager is responsible for waiting for the actual termination of the thread.
 */
void TCPSocketResponder::exitThread(void) {
	LOG_DEBUG("Requesting shutdown!");
	stopRunning();
	closesocket(server);
}
