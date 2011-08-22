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
#include <boost/asio/buffer.hpp>

#include <swap_bytes.hpp>
#include <unicode_char.hpp>
#include <strEx.h>
#include <utils.h>

#include <protobuf/envelope.pb.h>

using namespace nscp::helpers;

namespace nscp {



	class data {
	public:
		static const short unknown_packet = 0;
		static const short envelope_request = 1;
		static const short envelope_response = 2;

		static const short command_request = 10;
		static const short command_response = 11;

		static const short version_1 = 1;

		struct signature_packet {
			int16_t   version;

			int16_t   header_type;
			u_int32_t header_length;

			int16_t   payload_type;
			u_int32_t payload_length;

			u_int32_t additional_packet_count;

			signature_packet() {}
			signature_packet(const signature_packet &other) 
				: version(other.version)
				, header_type(other.header_type)
				, header_length(other.header_length)
				, payload_type(other.payload_type)
				, payload_length(other.payload_length)
				, additional_packet_count(other.additional_packet_count)
			{}
			const signature_packet& operator=(const signature_packet &other) {
				version = other.version;
				header_type = other.header_type;
				header_length = other.header_length;
				payload_type = other.payload_type;
				payload_length = other.payload_length;
				additional_packet_count = other.additional_packet_count;

				return *this;
			}

			std::wstring to_wstring() {
				std::wstringstream ss;
				ss << _T("version: ") << version 
					<< _T(", header: ") << header_type
					<< _T(", ") << header_length
					<< _T(", payload: ") << payload_type
					<< _T(", ") << payload_length
					<< _T(", count: ") << additional_packet_count ;
				return ss.str();
			}
		};
		struct raw_header_packet {
			char buffer[];
		};

	};
	struct length {
		static unsigned long get_signature_size() {
			return sizeof(data::signature_packet);
		}
		static unsigned long get_header_size(const data::signature_packet &signature) {
			return signature.header_length*sizeof(char);
		}
		static unsigned long get_payload_size(const data::signature_packet &signature) {
			return signature.payload_length*sizeof(char);
		}

	};

	class nscp_exception {
		std::wstring error_;
	public:
		nscp_exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};

	class packet /*: public boost::noncopyable*/ {
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

		struct nscp_chunk {
			nscp::data::signature_packet signature;
			std::string header;
			std::string payload;
			std::string to_buffer() const {
				std::string ret = write_signature();
				if (!header.empty())
					ret.insert(ret.end(), header.begin(), header.end());
				if (!payload.empty())
					ret.insert(ret.end(), payload.begin(), payload.end());
				return ret;
			}
			void read_signature(std::vector<char> &buf) {
				assert(buf.size() >= sizeof(nscp::data::signature_packet));
				nscp::data::signature_packet *tmp = reinterpret_cast<nscp::data::signature_packet*>(&(*buf.begin()));
				signature = *tmp;
				signature.payload_type = tmp->payload_type;
				signature.payload_length = tmp->payload_length;
			}
			void read_payload(std::vector<char> &buf) {
				payload = std::string(buf.begin(), buf.end());
			}
			std::string write_signature() const {
				char * buffer = new char[sizeof(nscp::data::signature_packet)+1];
				nscp::data::signature_packet *tmp = reinterpret_cast<nscp::data::signature_packet*>(buffer);
				*tmp = signature;
				std::string str_buf(buffer, sizeof(nscp::data::signature_packet));
				delete [] buffer;
				return str_buf;
			}

		};

		static nscp_chunk build_envelope_request(unsigned long additionl_packets) {
			nscp_chunk chunk;
			chunk.signature.header_length = 0;
			chunk.signature.header_type = 0;

			chunk.signature.additional_packet_count = additionl_packets;
			chunk.signature.version = nscp::data::version_1;

			NSCPEnvelope::Request request_envelope;
			request_envelope.set_version(NSCPEnvelope::VERSION_1);
			request_envelope.set_max_supported_version(NSCPEnvelope::VERSION_1);
			request_envelope.SerializeToString(&chunk.payload);

			chunk.signature.payload_length = chunk.payload.size();
			chunk.signature.payload_type = nscp::data::envelope_request;

			return chunk;
		}

		static nscp_chunk build_payload(unsigned long payload_type, std::string buffer, unsigned long additionl_packets) {
			nscp_chunk chunk;
			chunk.signature.header_length = 0;
			chunk.signature.header_type = 0;

			chunk.signature.additional_packet_count = additionl_packets;
			chunk.signature.version = nscp::data::version_1;

			chunk.payload = std::string(buffer.begin(), buffer.end());
			chunk.signature.payload_length = chunk.payload.size();
			chunk.signature.payload_type = payload_type;

			return chunk;
		}

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
			,type_(nscp::data::unknown_packet)
			,version_(nscp::data::version_1)
			,result_(0)
			,payload_length_(0)
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
			return packet(nscp::data::envelope_request, nscp::data::version_1, -1, payload, buffer_length);
		}

		const char* create_buffer() {
			return NULL;
		}

		std::vector<char> get_buffer() {
			const char *c = create_buffer();
			std::vector<char> buf(c, c+get_packet_length());
			return buf;
		}

		void readFrom(const char *buffer, unsigned int length) {
		}

		unsigned short getVersion() const { return version_; }
		unsigned short getType() const { return type_; }
		unsigned short getResult() const { return result_; }
		std::wstring getPayload() const { return payload_; }
		bool verifyCRC() { return calculatedCRC32_ == crc32_; }
		unsigned int get_packet_length() const { return 0; }
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
		static nscp::packet create_response(int ret, std::wstring string, int buffer_length) {
			return packet(nscp::data::unknown_packet, nscp::data::version_1, ret, string, buffer_length);
		}
	};
}

