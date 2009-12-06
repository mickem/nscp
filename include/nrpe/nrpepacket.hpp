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


namespace nrpe {
	// this function swap the bytes of values given it's size as a template
	// parameter (could sizeof be used?).
	template <class T, unsigned int size>
	inline T SwapBytes(T value) {
		union {
			T value;
			char bytes[size];
		} in, out;

		in.value = value;

		for (unsigned int i = 0; i < size / 2; ++i) {
			out.bytes[i] = in.bytes[size - 1 - i];
			out.bytes[size - 1 - i] = in.bytes[i];
		}

		return out.value;
	}

	template<EEndian from, EEndian to, class T>
	inline T EndianSwapBytes(T value) {
		BOOST_STATIC_ASSERT(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
		BOOST_STATIC_ASSERT(boost::is_arithmetic<T>::value);
		if (from == to)
			return value;
		return SwapBytes<T, sizeof(T)>(value);
	}
	template<class T>
	inline T ntoh(T value) {
		std::cout << "Swaping (in): " << value << " => " << EndianSwapBytes<EEndian::BIG_ENDIAN_ORDER, EEndian::HOST_ENDIAN_ORDER, T>(value) << std::endl;
		return EndianSwapBytes<EEndian::BIG_ENDIAN_ORDER, EEndian::HOST_ENDIAN_ORDER, T>(value);
	}
	template<class T>
	inline T hton(T value) {
		std::cout << "Swaping (out): " << value << " => " << EndianSwapBytes<EEndian::HOST_ENDIAN_ORDER, EEndian::BIG_ENDIAN_ORDER, T>(value) << std::endl;
		return EndianSwapBytes<EEndian::HOST_ENDIAN_ORDER, EEndian::BIG_ENDIAN_ORDER, T>(value);
	}



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
			std::cout << "get_packet: " << sizeof(nrpe::data::packet) << ":" << payload_length << std::endl;
			return sizeof(nrpe::data::packet)+payload_length*sizeof(char);
		}
		static size_type get_payload_length() {
			return payload_length_;
		}
		static size_type get_payload_length(size_type packet_length) {
			std::cout << "get_payload: " << sizeof(nrpe::data::packet) << ":" << packet_length << std::endl;
			return (packet_length-sizeof(nrpe::data::packet))/sizeof(char);
		}
	};

	class nrpe_packet_exception {
		std::wstring error_;
	public:
		nrpe_packet_exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};

	class packet {
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
			p->result_code = nrpe::hton<int16_t>(result_);
			p->packet_type = nrpe::hton<int16_t>(type_);
			p->packet_version = nrpe::hton<int16_t>(version_);
			if (payload_.length() >= payload_length_-1)
				throw nrpe::nrpe_packet_exception(_T("To much data cant create return packet (truncate datat)"));
			//ZeroMemory(p->buffer, payload_length_-1);
			strncpy(p->buffer, ::to_string(payload_).c_str(), payload_.length());
			p->buffer[payload_.length()] = 0;
			p->crc32_value = 0;
			p->crc32_value = nrpe::hton<u_int32_t>(calculate_crc32(tmpBuffer, packet_length));
			std::wcout << _T("About to send: ") << to_string() << std::endl;
			std::wcout << _T("About to send: ") 
				<< _T("") << strEx::ihextos(tmpBuffer[0]) 
				<< _T(", ") << strEx::ihextos(tmpBuffer[1]) 
				<< _T(", ") << strEx::ihextos(tmpBuffer[2]) 
				<< _T(", ") << strEx::ihextos(tmpBuffer[3]) 
				<< std::endl;
			return tmpBuffer;
		}

		void readFrom(const char *buffer, unsigned int length) {
			std::wcout << _T("Just read: ") 
				<< _T("") << strEx::ihextos(buffer[0]) 
				<< _T(", ") << strEx::ihextos(buffer[1]) 
				<< _T(", ") << strEx::ihextos(buffer[2]) 
				<< _T(", ") << strEx::ihextos(buffer[3]) 
				<< std::endl;
			if (buffer == NULL)
				throw nrpe::nrpe_packet_exception(_T("No buffer."));
			if (length != get_packet_length())
				throw nrpe::nrpe_packet_exception(_T("Invalid packet length: ") + strEx::itos(length) + _T(" != ") + strEx::itos(get_packet_length()) + _T(" configured payload is: ") + to_wstring(get_payload_length()));
			const nrpe::data::packet *p = reinterpret_cast<const nrpe::data::packet*>(buffer);
			type_ = nrpe::ntoh<int16_t>(p->packet_type);
			if ((type_ != nrpe::data::queryPacket)&&(type_ != nrpe::data::responsePacket))
				throw nrpe::nrpe_packet_exception(_T("Invalid packet type: ") + strEx::itos(type_));
			version_ = nrpe::ntoh<int16_t>(p->packet_version);
			if (version_ != nrpe::data::version2)
				throw nrpe::nrpe_packet_exception(_T("Invalid packet version.") + strEx::itos(version_));
			crc32_ = nrpe::ntoh<u_int32_t>(p->crc32_value);
			// Verify CRC32
			// @todo Fix this, currently we need a const buffer so we cannot change the CRC to 0.
			char * tb = new char[length+1];
			memcpy(tb, buffer, length);
			nrpe::data::packet *p2 = reinterpret_cast<nrpe::data::packet*>(tb);
			p2->crc32_value = 0;
			calculatedCRC32_ = calculate_crc32(tb, get_packet_length());
			delete [] tb;
			std::wcout << _T("Just read: ") << to_string() << std::endl;
			if (crc32_ != calculatedCRC32_) 
				throw nrpe::nrpe_packet_exception(_T("Invalid checksum in NRPE packet: ") + strEx::ihextos(crc32_) 
				+ _T("!=") + strEx::ihextos(calculatedCRC32_));
			// Verify CRC32 end
			result_ = nrpe::ntoh<u_int32_t>(p->result_code);
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
			ss << _T(", payload: ") << payload_;
			return ss.str();
		}
		static nrpe::packet create_response(int ret, std::wstring string, int buffer_length) {
			return packet(nrpe::data::responsePacket, nrpe::data::version2, ret, string, buffer_length);
		}
	};
}

