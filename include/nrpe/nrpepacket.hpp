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


typedef short int16_t;
typedef unsigned long u_int32_t;

class NRPEPacket {
public:
	static const short unknownPacket = 0;
	static const short queryPacket = 1;
	static const short responsePacket = 2;
	static const short version2 = 2;

	class NRPEPacketException {
		std::wstring error_;
	public:
		NRPEPacketException(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};

private:
	typedef struct packet {
		int16_t   packet_version;
		int16_t   packet_type;
		u_int32_t crc32_value;
		int16_t   result_code;
		char      buffer[];
	} packet;
	std::wstring payload_;
	short type_;
	short version_;
	NSCAPI::nagiosReturn result_;
	unsigned int crc32_;
	unsigned int calculatedCRC32_;
	char *tmpBuffer;
	unsigned int buffer_length_;
public:
	NRPEPacket(unsigned int buffer_length) : tmpBuffer(NULL), buffer_length_(buffer_length) {};
	NRPEPacket(const char *buffer, unsigned int length, unsigned int buffer_length) : tmpBuffer(NULL), buffer_length_(buffer_length) {
		readFrom(buffer, length);
	};
	NRPEPacket(short type, short version, NSCAPI::nagiosReturn result, std::wstring payLoad, unsigned int buffer_length) 
		: tmpBuffer(NULL) 
		,type_(type)
		,version_(version)
		,result_(result)
		,payload_(payLoad)
		,buffer_length_(buffer_length)
	{
	}
	NRPEPacket() 
		: tmpBuffer(NULL) 
		,type_(unknownPacket)
		,version_(version2)
		,result_(0)
		,buffer_length_(0)
	{
	}
	NRPEPacket(NRPEPacket &other) : tmpBuffer(NULL) {
		payload_ = other.payload_;
		type_ = other.type_;
		version_ = other.version_;
		result_ = other.result_;
		crc32_ = other.crc32_;
		calculatedCRC32_ = other.calculatedCRC32_;
		buffer_length_ = other.buffer_length_;
	}
	~NRPEPacket() {
		delete [] tmpBuffer;
	}
	static NRPEPacket make_request(std::wstring payload, unsigned int buffer_length) {
		return NRPEPacket(queryPacket, version2, -1, payload, buffer_length);
	}

	const char* getBuffer() {
		delete [] tmpBuffer;
		tmpBuffer = new char[getBufferLength()+1];
		ZeroMemory(tmpBuffer, getBufferLength()+1);
		packet *p = reinterpret_cast<packet*>(tmpBuffer);
		p->result_code = htons(NSCHelper::nagios2int(result_));
		p->packet_type = htons(type_);
		p->packet_version = htons(version_);
		if (payload_.length() >= buffer_length_-1)
			throw NRPEPacketException(_T("To much data cant create return packet (truncate datat)"));
		//ZeroMemory(p->buffer, buffer_length_-1);
		strncpy_s(p->buffer, buffer_length_-1, strEx::wstring_to_string(payload_).c_str(), payload_.length());
		p->buffer[payload_.length()] = 0;
		p->crc32_value = 0;
		p->crc32_value = htonl(calculate_crc32(tmpBuffer, getBufferLength()));
		return tmpBuffer;
	}

	void readFrom(const char *buffer, unsigned int length) {
		if (buffer == NULL)
			throw NRPEPacketException(_T("No buffer."));
		if (length != getBufferLength())
			throw NRPEPacketException(_T("Invalid length: ") + strEx::itos(length) + _T(" != ") + strEx::itos(getBufferLength()));
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
		char * tb = new char[getBufferLength()+1];
		memcpy(tb, buffer, getBufferLength());
		packet *p2 = reinterpret_cast<packet*>(tb);
		p2->crc32_value = 0;
		calculatedCRC32_ = calculate_crc32(tb, getBufferLength());
		delete [] tb;
		if (crc32_ != calculatedCRC32_) 
			throw NRPEPacketException(_T("Invalid checksum in NRPE packet!"));
		// Verify CRC32 end
		result_ = NSCHelper::int2nagios(ntohs(p->result_code));
		payload_ = strEx::string_to_wstring(std::string(p->buffer));
	}

	unsigned short getVersion() const { return version_; }
	unsigned short getType() const { return type_; }
	unsigned short getResult() const { return result_; }
	std::wstring getPayload() const { return payload_; }
	bool verifyCRC() { return calculatedCRC32_ == crc32_; }
	unsigned int getBufferLength() const { return getBufferLength(buffer_length_); }
	static unsigned int getBufferLength(unsigned int buffer_length) { return sizeof(packet)+buffer_length*sizeof(char); }
	unsigned int getInternalBufferLength() const { return buffer_length_; }


	std::wstring toString() {
		std::wstringstream ss;
		ss << _T("type: ") << type_;
		ss << _T(", version: ") << version_;
		ss << _T(", result: ") << result_;
		ss << _T(", payload: ") << payload_;
		return ss.str();
	}

};




