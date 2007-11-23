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
#include "stdafx.h"
#include <WinSock2.h>
#include "NRPEPacket.h"
#include <msvc_wrappers.h>

const char* NRPEPacket::getBuffer() {
	delete [] tmpBuffer;
	tmpBuffer = new char[getBufferLength()];
	packet *p = reinterpret_cast<packet*>(tmpBuffer);
	p->result_code = htons(NSCHelper::nagios2int(result_));
	p->packet_type = htons(type_);
	p->packet_version = htons(version_);
	p->crc32_value = 0;
	strncpy_s(p->buffer, 1024, strEx::wstring_to_string(payload_).c_str(), 1023);
	p->buffer[1024] = 0;
	p->crc32_value = htonl(calculate_crc32(tmpBuffer, getBufferLength()));
	return tmpBuffer;
}

void NRPEPacket::readFrom(const char *buffer, unsigned int length) {
	if (buffer == NULL)
		throw NRPEPacketException(_T("No buffer."));
	if (length != sizeof(packet))
		throw NRPEPacketException(_T("Invalid length."));
	const packet *p = reinterpret_cast<const packet*>(buffer);
	type_ = ntohs(p->packet_type);
	if ((type_ != queryPacket)&&(type_ != responsePacket))
		throw NRPEPacketException(_T("Invalid packet type."));
	version_ = ntohs(p->packet_version);
	if (version_ != version2)
		throw NRPEPacketException(_T("Invalid packet version."));
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
	payload_ = strEx::string_to_wstring(std::string(p->buffer));
}



