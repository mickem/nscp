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

static unsigned long crc32_table[256];

/* build the crc table - must be called before calculating the crc value */
void generate_crc32_table(void){
	unsigned long crc, poly;
	int i, j;

	poly=0xEDB88320L;
	for(i=0;i<256;i++){
		crc=i;
		for(j=8;j>0;j--){
			if(crc & 1)
				crc=(crc>>1)^poly;
			else
				crc>>=1;
		}
		crc32_table[i]=crc;
	}

	return;
}

/* calculates the CRC 32 value for a buffer */
unsigned long calculate_crc32(char *buffer, int buffer_size){
	register unsigned long crc;
	int this_char;
	int current_index;

	crc=0xFFFFFFFF;

	for(current_index=0;current_index<buffer_size;current_index++){
		this_char=(int)buffer[current_index];
		crc=((crc>>8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
	}

	return (crc ^ 0xFFFFFFFF);
}

void NRPESocket::onAccept(SOCKET client) {
	NSC_DEBUG_MSG("Accepting connection from remote host");

	SimpleSocketListsner::readAllDataBlock block = SimpleSocketListsner::readAll(client);
	packet *p = reinterpret_cast<packet*>(block.first);
	// @todo Verify versions and stuff, and ofcource add SSL (but thats in the future :)
	NSC_DEBUG_MSG_STD("Incoming data: " + p->buffer);

	charEx::token cmd = charEx::getToken(p->buffer, '!');
	std::string msg, perf;
	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first.c_str(), cmd.second, '!', msg, perf);
//QUERY_PACKET
	packet p2;
	strncpy(p2.buffer, (msg+"|"+perf).c_str(), 1023);
	p2.buffer[1023] = 0;
	p2.packet_type = htons(2);
	p2.packet_version = htons(2);
	p2.result_code = htons(static_cast<int>(ret));

	generate_crc32_table();
	p2.crc32_value = 0;
	p2.crc32_value = htonl(calculate_crc32(reinterpret_cast<char*>(&p2),sizeof(p2)));
	send(client, reinterpret_cast<char*>(&p2), sizeof(p2), 0);

	delete [] block.first;
	closesocket(client);
}

