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
	static const short queryPacket = 1;
	static const short responsePacket = 2;
	static const short version2 = 2;

	class NRPEPacketException {
		std::string error_;
	public:
		NRPEPacketException(std::string error) : error_(error) {}
		std::string getMessage() {
			return error_;
		}
	};

private:
	typedef struct packet {
		int16_t   packet_version;
		int16_t   packet_type;
		u_int32_t crc32_value;
		int16_t   result_code;
		char      buffer[1024];
	} packet;
	std::string payload_;
	short type_;
	short version_;
	NSCAPI::nagiosReturn result_;
	unsigned int crc32_;
	unsigned int calculatedCRC32_;
	char *tmpBuffer;
public:
	NRPEPacket() : tmpBuffer(NULL) {};
	NRPEPacket(const char *buffer, unsigned int length) : tmpBuffer(NULL) {
		readFrom(buffer, length);
	};
	NRPEPacket(short type, short version, NSCAPI::nagiosReturn result, std::string payLoad) 
		: tmpBuffer(NULL) 
		,type_(type)
		,version_(version)
		,result_(result)
		,payload_(payLoad)
	{
	}
	NRPEPacket(NRPEPacket &other) : tmpBuffer(NULL) {
		payload_ = other.payload_;
		type_ = other.type_;
		version_ = other.version_;
		result_ = other.result_;
		crc32_ = other.crc32_;
		calculatedCRC32_ = other.calculatedCRC32_;
	}
	~NRPEPacket() {
		delete [] tmpBuffer;
	}
	void readFrom(const char *buffer, unsigned int length);
	unsigned short getVersion() const { return version_; }
	unsigned short getType() const { return type_; }
	unsigned short getResult() const { return result_; }
	std::string getPayload() const { return payload_; }
	const char* getBuffer();
	bool verifyCRC() {
		return calculatedCRC32_ == crc32_;
	}
	static unsigned int getBufferLength() {
		return sizeof(packet);
	}
};



