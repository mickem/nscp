#include "stdafx.h"
#include "strEx.h"
#include "NRPESocket.h"

/**
 * Default c-tor
 */
NRPESocket::NRPESocket(): SimpleSocketListsner(DEFAULT_NRPE_PORT) {
}

NRPESocket::~NRPESocket() {
}

typedef short int16_t;
typedef unsigned long u_int32_t;

typedef struct packet_struct{
	int16_t   packet_version;
	int16_t   packet_type;
	u_int32_t crc32_value;
	int16_t   result_code;
	char      buffer[1024];
}packet;

void NRPESocket::onAccept(SOCKET client) {
	NSC_DEBUG_MSG("Accepting connection from remote host");

	SimpleSocketListsner::readAllDataBlock block = SimpleSocketListsner::readAll(client);
	packet *p = reinterpret_cast<packet*>(block.first);
	/*
	char* foo = new char[1024];
	sprintf(foo, "packet_version = %d, packet_type = %d, crc32 = %d, result_code = %d",
		ntohs(p->packet_version), ntohs(p->packet_type), ntohl(p->crc32_value), ntohs(p->result_code));

	NSC_DEBUG_MSG_STD("Incoming header: " + foo);
	*/
	// @todo Verify versions and stuff, and ofcource add SSL (but thats in the future :)
	NSC_DEBUG_MSG_STD("Incoming data: " + p->buffer);

	charEx::token cmd = charEx::getToken(p->buffer, '!');
	std::string msg, perf;
	NSCModuleHelper::InjectSplitAndCommand(cmd.first.c_str(), cmd.second, '!', msg, perf);

	delete [] block.first;
	closesocket(client);
}

