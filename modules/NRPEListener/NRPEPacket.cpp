#include "stdafx.h"
#include <WinSock2.h>
#include "NRPEPacket.h"

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

void NRPEPacket::readFrom(const char *buffer, unsigned int length) {
	if (buffer == NULL)
		throw NRPEPacketException("No buffer.");
	if (length != sizeof(packet))
		throw NRPEPacketException("Invalid length.");
	const packet *p = reinterpret_cast<const packet*>(buffer);
	type_ = ntohs(p->packet_type);
	if ((type_ != queryPacket)&&(type_ != responsePacket))
		throw NRPEPacketException("Invalid packet type.");
	version_ = ntohs(p->packet_version);
	if (version_ != version2)
		throw NRPEPacketException("Invalid packet version.");
	crc32_ = ntohl(p->crc32_value);
	// Verify CRC32
	// @todo Fix this, currently we need a const buffer so we cannot change the CRC to 0.
	char * tb = new char[getBufferLength()];
	memcpy(tb, buffer, getBufferLength());
	packet *p2 = reinterpret_cast<packet*>(tb);
	p2->crc32_value = 0;
	calculatedCRC32_ = calculate_crc32(tb, getBufferLength());
	delete [] tb;
	// Verify CRC32 end
	result_ = NSCHelper::int2nagios(ntohs(p->result_code));
	payload_ = std::string(p->buffer);
}



