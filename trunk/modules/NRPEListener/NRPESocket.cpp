#include "stdafx.h"
#include "strEx.h"
#include "NRPESocket.h"





const char* NRPEPacket::getBuffer() {
	delete [] tmpBuffer;
	tmpBuffer = new char[getBufferLength()];
	packet *p = reinterpret_cast<packet*>(tmpBuffer);
	p->result_code = htons(NSCHelper::nagios2int(result_));
	p->packet_type = htons(type_);
	p->packet_version = htons(version_);
	p->crc32_value = 0;
	strncpy(p->buffer, payload_.c_str(), 1023);
	p->buffer[1024] = 0;
	p->crc32_value = htonl(calculate_crc32(tmpBuffer, getBufferLength()));
	return tmpBuffer;
}

