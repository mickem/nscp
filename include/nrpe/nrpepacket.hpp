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

#include <string>

typedef short int16_t;
typedef unsigned long u_int32_t;


class NRPEException {
	std::wstring error_;
public:
/*		NRPESocketException(simpleSSL::SSLException e) {
		error_ = e.getMessage();
	}
	NRPEException(NRPEPacket::NRPEPacketException e) {
		error_ = e.getMessage();
	}
	*/
	NRPEException(std::wstring s) {
		error_ = s;
	}
	std::wstring getMessage() {
		return error_;
	}
};

class NRPEPacket {
public:
	static const short unknownPacket = 0;
	static const short queryPacket = 1;
	static const short responsePacket = 2;
	static const short extendedResponsePacket = 3;
	static const short extendedQueryPacket = 4;
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
		if (payload_.length() > buffer_length_-1)
			throw NRPEPacketException(_T("To much data cant create return packet (truncate data): ") + strEx::itos(payload_.length()));
		//ZeroMemory(p->buffer, buffer_length_-1);
		strncpy_s(p->buffer, buffer_length_, strEx::wstring_to_string(payload_).c_str(), payload_.length());
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


	std::wstring toString() {
		std::wstringstream ss;
		ss << _T("type: ") << type_;
		ss << _T(", version: ") << version_;
		ss << _T(", result: ") << result_;
		ss << _T(", payload: ") << payload_;
		return ss.str();
	}

	bool is_last() const {
		return getType() != extendedQueryPacket;
	}

	void set_type( short type ) {
		type_ = type;
	}

};

class NRPEData {
private:
	typedef std::vector<NRPEPacket*> packet_list;
	packet_list packets;
	unsigned int payload_size_;


public:

	NRPEData(unsigned int payload_size)
		: payload_size_(payload_size)
	{};
// 	NRPEData(char *buffer, unsigned int length, unsigned int payload_size) 
// 		 : payload_size_(payload_size)
// 		 , allow_multiple_packets_(false) 
// 	{
// 		packets.push_back(new NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, buffer, length, payload_size));
// 	}
	NRPEData(NSCAPI::nagiosReturn result, std::wstring payload, unsigned int payload_size) 
		: payload_size_(payload_size)
	{
		if (payload.size() >= payload_size_) {
			int i=0;
			unsigned int sz_each = payload_size_-1;
			for (int i=0;i<payload.size();i+=sz_each) {
				int len = payload.size()>sz_each?sz_each:payload.length();
				std::wstring p = payload.substr(i, len);
				packets.push_back(new NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, result, p, payload_size_));
			}
		} else
			packets.push_back(new NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, result, payload, payload_size_));
	}
	NRPEData(NRPEData &other) {
		packets = other.steal_packets();
		payload_size_ = other.payload_size_;
	}
	void operator=(NRPEData &other) {
		packets = other.steal_packets();
		payload_size_ = other.payload_size_;
	}

	packet_list steal_packets() {
		packet_list ret = packets;
		packets.clear();
		return ret;
	}


	~NRPEData() {
		erase_all();
	}
	void erase_all() {
		for (packet_list::iterator it = packets.begin(); it != packets.end(); ++it) {
			delete (*it);
		}
		packets.clear();
	}


	std::wstring::size_type size() const { return packets.size(); }
	unsigned int payload_size() const { return payload_size_; }

	std::wstring toString() {
		std::wstringstream ss;
		int i=0;
		for (packet_list::iterator it = packets.begin(); it != packets.end(); ++it) {
			ss << _T("[") << i++ << _T("] ") << (*it)->toString();
		}
		return ss.str();
	}

	void validate_query() {
		for (packet_list::iterator it = packets.begin(); it != packets.end(); ++it) {
			if ((*it)->getType() != NRPEPacket::queryPacket && (*it)->getType() != NRPEPacket::extendedQueryPacket) {
				NSC_LOG_ERROR(_T("Request is not a query."));
				throw NRPEException(_T("Invalid query type: ") + strEx::itos((*it)->getType()));
			}
			if ((*it)->getVersion() != NRPEPacket::version2) {
				NSC_LOG_ERROR(_T("Request had unsupported version."));
				throw NRPEException(_T("Invalid version"));
			}
			if (!(*it)->verifyCRC()) {
				NSC_LOG_ERROR(_T("Request had invalid checksum."));
				throw NRPEException(_T("Invalid checksum"));
			}
		}
	}
	bool read(const char * buffer, const unsigned int len) {
		NRPEPacket *d = new NRPEPacket(buffer, len, payload_size_);
		packets.push_back(d);
		return !d->is_last();
	}

	std::wstring getPayload() {
		std::wstring s;
		for (packet_list::iterator it = packets.begin(); it != packets.end(); ++it) {
			s += (*it)->getPayload();
		}
		return s;
	}

	unsigned int get_packet_length() {
		return NRPEPacket::getBufferLength(payload_size_);
	}
	void set_error(NSCAPI::nagiosReturn result, std::wstring payload) {
		erase_all();
		packets.push_back(new NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, result, payload, payload_size_));
	}

	bool write(unsigned int index, simpleSocket::DataBuffer &block, bool last) {
		if (index >= packets.size())
			throw NRPEException(_T("Trying to read non existing packet"));
		NRPEPacket *current = packets[index];
		current->set_type(last?NRPEPacket::responsePacket:NRPEPacket::extendedResponsePacket);
		block.copyFrom(current->getBuffer(), current->getBufferLength());
		return index == packets.size()-1;
	}


};



