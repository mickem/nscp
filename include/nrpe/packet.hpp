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

#include <types.hpp>
#include <string>
#include <unicode_char.hpp>
#include <boost/asio/buffer.hpp>
#include <swap_bytes.hpp>
#include <strEx.h>
#include <utils.h>

using namespace nscp::helpers;

namespace nrpe {



	class data {
	public:
		static const short unknownPacket = 0;
		static const short queryPacket = 1;
		static const short responsePacket = 2;
		static const short version2 = 2;

		typedef struct packet {
			int16_t   packet_version;
			int16_t   packet_type;
			u_int32_t crc32_value;
			int16_t   result_code;
			char      buffer[];
		} packet;

	};

	class length {
		typedef unsigned int size_type;
		static size_type payload_length_;
	public:
		static void set_payload_length(size_type length) {
			payload_length_ = length;
		}
		static size_type get_packet_length() {
			return get_packet_length(payload_length_);
		}
		static size_type get_packet_length(size_type payload_length) {
			return sizeof(nrpe::data::packet)+payload_length*sizeof(char);
		}
		static size_type get_payload_length() {
			return payload_length_;
		}
		static size_type get_payload_length(size_type packet_length) {
			return (packet_length-sizeof(nrpe::data::packet))/sizeof(char);
		}
	};

	class nrpe_exception : public std::exception {
		std::string error_;
	public:
		nrpe_exception(std::wstring error) : error_(utf8::cvt<std::string>(error)) {}
		nrpe_exception(std::string error) : error_(utf8::cvt<std::string>(error)) {}
		~nrpe_exception() throw() {}
		const char* what() const throw() {
			return error_.c_str();
		}
		const std::wstring wwhat() const throw() {
			return utf8::cvt<std::wstring>(error_);
		}
	};
	class nrpe_packet_exception : public nrpe_exception {
	public:
		nrpe_packet_exception(std::wstring error) : nrpe_exception(error) {}
	};

	class packet /*: public boost::noncopyable*/ {
	public:


	private:
		std::wstring payload_;
		short type_;
		short version_;
		int result_;
		unsigned int crc32_;
		unsigned int calculatedCRC32_;
		char *tmpBuffer;
		unsigned int payload_length_;
	public:
		packet(unsigned int payload_length) : tmpBuffer(NULL), payload_length_(payload_length) {};
		packet(std::vector<char> buffer, unsigned int payload_length) : tmpBuffer(NULL), payload_length_(payload_length) {
			char *tmp = new char[buffer.size()+1];
			copy( buffer.begin(), buffer.end(), tmp);
			readFrom(tmp, buffer.size());
			delete [] tmp;
		};
		packet(const char *buffer, unsigned int buffer_length, unsigned int payload_length) : tmpBuffer(NULL), payload_length_(payload_length) {
			readFrom(buffer, buffer_length);
		};
		packet(short type, short version, int result, std::wstring payLoad, unsigned int payload_length) 
			: tmpBuffer(NULL) 
			,type_(type)
			,version_(version)
			,result_(result)
			,payload_(payLoad)
			,payload_length_(payload_length)
		{
		}
		packet() 
			: tmpBuffer(NULL) 
			,type_(nrpe::data::unknownPacket)
			,version_(nrpe::data::version2)
			,result_(0)
			,payload_length_(nrpe::length::get_payload_length())
		{
		}
		packet(const packet &other) : tmpBuffer(NULL) {
			payload_ = other.payload_;
			type_ = other.type_;
			version_ = other.version_;
			result_ = other.result_;
			crc32_ = other.crc32_;
			calculatedCRC32_ = other.calculatedCRC32_;
			payload_length_ = other.payload_length_;
		}
		packet& operator=(packet const& other) {
			tmpBuffer=NULL;
			payload_ = other.payload_;
			type_ = other.type_;
			version_ = other.version_;
			result_ = other.result_;
			crc32_ = other.crc32_;
			calculatedCRC32_ = other.calculatedCRC32_;
			payload_length_ = other.payload_length_;
			return *this;
		}

		~packet() {
			delete [] tmpBuffer;
		}
		static packet make_request(std::wstring payload, unsigned int buffer_length) {
			return packet(nrpe::data::queryPacket, nrpe::data::version2, -1, payload, buffer_length);
		}

		const char* create_buffer() {
			delete [] tmpBuffer;
			unsigned int packet_length = nrpe::length::get_packet_length(payload_length_);
			tmpBuffer = new char[packet_length+1];
			//TODO readd this ZeroMemory(tmpBuffer, getBufferLength()+1);
			nrpe::data::packet *p = reinterpret_cast<nrpe::data::packet*>(tmpBuffer);
			p->result_code = swap_bytes::hton<int16_t>(result_);
			p->packet_type = swap_bytes::hton<int16_t>(type_);
			p->packet_version = swap_bytes::hton<int16_t>(version_);
			if (payload_.length() >= payload_length_-1)
				throw nrpe::nrpe_packet_exception(_T("To much data cant create return packet (truncate datat)"));
			//ZeroMemory(p->buffer, payload_length_-1);
			strncpy(p->buffer, ::to_string(payload_).c_str(), payload_.length());
			p->buffer[payload_.length()] = 0;
			p->crc32_value = 0;
			crc32_ = p->crc32_value = swap_bytes::hton<u_int32_t>(calculate_crc32(tmpBuffer, packet_length));
// 			std::wcout << _T("About to send: ") << to_string() << std::endl;
// 			std::wcout << _T("About to send: ") 
// 				<< _T("<<<") << to_wstring(strEx::format_buffer(tmpBuffer, packet_length)) 
// 				<< _T(">>>, ") << strEx::ihextos((int16_t)tmpBuffer[2]) 
// 				<< _T(", ") << strEx::ihextos((u_int32_t)tmpBuffer[4]) 
// 				<< _T(", ") << strEx::ihextos((int16_t)tmpBuffer[8]) 
// 				<< _T(", crc: ") << strEx::ihextos(p->crc32_value) 
// 				<< std::endl;
			return tmpBuffer;
		}

		std::vector<char> get_buffer() {
			const char *c = create_buffer();
			std::vector<char> buf(c, c+get_packet_length());
			return buf;
		}

		void readFrom(const char *buffer, unsigned int length) {
// 			std::wcout << _T("Just read: ") 
// 				<< _T("") << strEx::ihextos(buffer[0]) 
// 				<< _T(", ") << strEx::ihextos(buffer[1]) 
// 				<< _T(", ") << strEx::ihextos(buffer[2]) 
// 				<< _T(", ") << strEx::ihextos(buffer[3]) 
// 				<< _T(", ") << strEx::ihextos(buffer[4]) 
// 				<< std::endl;
			if (buffer == NULL)
				throw nrpe::nrpe_packet_exception(_T("No buffer."));
			if (length != get_packet_length())
				throw nrpe::nrpe_packet_exception(_T("Invalid packet length: ") + strEx::itos(length) + _T(" != ") + strEx::itos(get_packet_length()) + _T(" configured payload is: ") + to_wstring(get_payload_length()));
			const nrpe::data::packet *p = reinterpret_cast<const nrpe::data::packet*>(buffer);
			type_ = swap_bytes::ntoh<int16_t>(p->packet_type);
			if ((type_ != nrpe::data::queryPacket)&&(type_ != nrpe::data::responsePacket))
				throw nrpe::nrpe_packet_exception(_T("Invalid packet type: ") + strEx::itos(type_));
			version_ = swap_bytes::ntoh<int16_t>(p->packet_version);
			if (version_ != nrpe::data::version2)
				throw nrpe::nrpe_packet_exception(_T("Invalid packet version.") + strEx::itos(version_));
			crc32_ = swap_bytes::ntoh<u_int32_t>(p->crc32_value);
			// Verify CRC32
			// @todo Fix this, currently we need a const buffer so we cannot change the CRC to 0.
			char * tb = new char[length+1];
			memcpy(tb, buffer, length);
			nrpe::data::packet *p2 = reinterpret_cast<nrpe::data::packet*>(tb);
			p2->crc32_value = 0;
			calculatedCRC32_ = calculate_crc32(tb, get_packet_length());
			delete [] tb;
// 			std::wcout << _T("Just read: ") << to_string() << std::endl;
			if (crc32_ != calculatedCRC32_) 
				throw nrpe::nrpe_packet_exception(_T("Invalid checksum in NRPE packet: ") + strEx::ihextos(crc32_) + _T("!=") + strEx::ihextos(calculatedCRC32_));
			// Verify CRC32 end
			result_ = swap_bytes::ntoh<u_int32_t>(p->result_code);
			payload_ = strEx::string_to_wstring(std::string(p->buffer));
		}

		unsigned short getVersion() const { return version_; }
		unsigned short getType() const { return type_; }
		unsigned short getResult() const { return result_; }
		std::wstring getPayload() const { return payload_; }
		bool verifyCRC() { return calculatedCRC32_ == crc32_; }
		unsigned int get_packet_length() const { return nrpe::length::get_packet_length(payload_length_); }
		unsigned int get_payload_length() const { return payload_length_; }

		boost::asio::const_buffer to_buffers() {
			return boost::asio::buffer(create_buffer(), get_packet_length());
		}
		std::wstring to_string() {
			std::wstringstream ss;
			ss << _T("type: ") << type_;
			ss << _T(", version: ") << version_;
			ss << _T(", result: ") << result_;
			ss << _T(", crc32: ") << crc32_;
			ss << _T(", payload: ") << payload_;
			return ss.str();
		}
		static nrpe::packet create_response(int ret, std::wstring string, int buffer_length) {
			return packet(nrpe::data::responsePacket, nrpe::data::version2, ret, string, buffer_length);
		}
	};
}

