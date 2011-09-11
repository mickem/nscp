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

#include <protobuf/ipc.pb.h>
#include <protobuf/plugin.pb.h>

using namespace nscp::helpers;

namespace nscp {
	class data {
	public:
		static const short unknown_packet = 0;

		static const short envelope_request = 1;
		static const short envelope_response = 2;

		static const short command_request = 10;
		static const short command_response = 11;

		static const short exec_request = 20;
		static const short exec_response = 21;

		static const int nscp_magic_number = 12345;
		static const short error = 100;

		static const short version_1 = 1;

		struct signature_packet {
			int16_t   version;

			int16_t   header_type;
			unsigned long long  header_length;

			int16_t   payload_type;
			unsigned long long payload_length;

			u_int32_t additional_packet_count;
			u_int32_t magic_number;

			signature_packet() : magic_number(nscp_magic_number) {}
			signature_packet(const signature_packet &other) 
				: version(other.version)
				, header_type(other.header_type)
				, header_length(other.header_length)
				, payload_type(other.payload_type)
				, payload_length(other.payload_length)
				, magic_number(other.magic_number)
				, additional_packet_count(other.additional_packet_count)
			{}
			const signature_packet& operator=(const signature_packet &other) {
				version = other.version;
				header_type = other.header_type;
				header_length = other.header_length;
				payload_type = other.payload_type;
				payload_length = other.payload_length;
				additional_packet_count = other.additional_packet_count;
				magic_number = other.magic_number;
				return *this;
			}

			bool validate() const {
				return magic_number == nscp_magic_number;
			}

			std::wstring to_wstring() const {
				std::wstringstream ss;
				ss << _T("version: ") << version 
					<< _T(", magic: ") << magic_number
					<< _T(", header: ") << header_type
					<< _T(", ") << header_length
					<< _T(", payload: ") << payload_type
					<< _T(", ") << payload_length
					<< _T(", count: ") << additional_packet_count ;
				return ss.str();
			}
			std::string to_string() const {
				std::stringstream ss;
				ss << "version: " << version 
					<< ", magic: " << magic_number
					<< ", header: " << header_type
					<< ", " << header_length
					<< ", payload: " << payload_type
					<< ", " << payload_length
					<< ", count: " << additional_packet_count ;
				return ss.str();
			}
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

	class nscp_exception : public std::exception {
		std::string error_;
	public:
		nscp_exception(std::wstring error) : error_(utf8::cvt<std::string>(error)) {}
		nscp_exception(std::string error) : error_(error) {}
		virtual ~nscp_exception() throw() {}
		virtual const char* what() const throw() {
			return error_.c_str();
		}
	};


	struct packet {
		nscp::data::signature_packet signature;
		std::string header;
		std::string payload;

		packet() {}
		packet(const nscp::data::signature_packet &sig) : signature(sig) {}
		packet(const nscp::data::signature_packet &sig, std::string header, std::string payload) : signature(sig), header(header), payload(payload) {}
		packet(const packet & other) : signature(other.signature), header(other.header), payload(other.payload) {}
		const packet& operator=(const packet & other) {
			signature = other.signature;
			header = other.header;
			payload = other.payload;
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////
		// Write to string
		std::string write_string() const {
			std::string ret;
			write_signature(ret);
			write_header(ret);
			write_payload(ret);
			return ret;
		}
		void write_signature(std::string &buffer) const {
			// @todo: Optimize this away once this is working
			char * tmpbuffer = new char[length::get_signature_size()+1];
			nscp::data::signature_packet *tmp = reinterpret_cast<nscp::data::signature_packet*>(tmpbuffer);
			*tmp = signature;
			buffer.append(tmpbuffer, length::get_signature_size());
			delete [] tmpbuffer;
		}
		inline void write_header(std::string &buffer) const {
			if (!header.empty())
				buffer.insert(buffer.end(), header.begin(), header.end());
		}
		inline void write_payload(std::string &buffer) const {
			if (!payload.empty())
				buffer.insert(buffer.end(), payload.begin(), payload.end());
		}

		//////////////////////////////////////////////////////////////////////////
		// Read from vector (string?)
		void read_signature(std::vector<char> &buf) {
			assert(buf.size() >= nscp::length::get_signature_size());
			// @todo: Optimize this away once this is working
			nscp::data::signature_packet *tmp = reinterpret_cast<nscp::data::signature_packet*>(&(*buf.begin()));
			signature = *tmp;
			signature.payload_type = tmp->payload_type;
			signature.payload_length = tmp->payload_length;
		}
		void read_signature(std::string::iterator begin, std::string::iterator end) {
			assert(end-begin >= nscp::length::get_signature_size());
			// @todo: Optimize this away once this is working
			nscp::data::signature_packet *tmp = reinterpret_cast<nscp::data::signature_packet*>(&(*begin));
			signature = *tmp;
			signature.payload_type = tmp->payload_type;
			signature.payload_length = tmp->payload_length;
		}
		void nibble_signature(std::string &buf) {
			assert(buf.size() >= nscp::length::get_signature_size());
			// @todo: Optimize this away once this is working
			nscp::data::signature_packet *tmp = reinterpret_cast<nscp::data::signature_packet*>(&(*buf.begin()));
			signature = *tmp;
			signature.payload_type = tmp->payload_type;
			signature.payload_length = tmp->payload_length;
			buf.erase(buf.begin(), buf.begin()+nscp::length::get_signature_size());
		}
		void read_header(std::vector<char> &buf) {
			header = std::string(buf.begin(), buf.end());
		}
		void read_header(std::string::iterator begin, std::string::iterator end) {
			header = std::string(begin, end);
		}
		void nibble_header(std::string &buf) {
			assert(buf.size() >= nscp::length::get_header_size(signature));
			header = std::string(buf.begin(), buf.begin()+nscp::length::get_header_size(signature));
			buf.erase(buf.begin(), buf.begin()+nscp::length::get_header_size(signature));
		}
		void read_payload(std::vector<char> &buf) {
			payload = std::string(buf.begin(), buf.end());
		}
		void read_payload(std::string::iterator begin, std::string::iterator end) {
			payload = std::string(begin, end);
		}
		void nibble_payload(std::string &buf) {
			assert(buf.size() >= nscp::length::get_payload_size(signature));
			payload = std::string(buf.begin(), buf.begin()+nscp::length::get_payload_size(signature));
			buf.erase(buf.begin(), buf.begin()+nscp::length::get_payload_size(signature));
		}
		void read_all(std::string &buffer) {
			std::string::iterator begin = buffer.begin();
			std::string::iterator end = begin+length::get_signature_size();
			read_signature(begin, end);
			begin = end;
			end = begin+length::get_header_size(signature);
			read_header(begin, end);
			begin = end;
			end = begin+length::get_payload_size(signature);
			read_payload(begin, end);
		}

		std::wstring to_wstring() const {
			return signature.to_wstring();
		}
		std::string to_string() const {
			return signature.to_string();
		}

	};

	struct factory {


		static nscp::data::signature_packet create_simple_sig(int payload_type, std::string::size_type size) {
			nscp::data::signature_packet signature;
			signature.header_length = 0;
			signature.header_type = 0;

			signature.additional_packet_count = 0;
			signature.version = nscp::data::version_1;

			signature.payload_length = size;
			signature.payload_type = payload_type;
			return signature;

		}
		static packet create_payload(unsigned long payload_type, std::string buffer, unsigned long additional_packets = 0) {
			nscp::data::signature_packet signature;
			signature.header_length = 0;
			signature.header_type = 0;

			signature.additional_packet_count = additional_packets;
			signature.version = nscp::data::version_1;

			signature.payload_length = buffer.size();
			signature.payload_type = payload_type;

			return packet(signature, "", buffer);
		}

		static packet create_query_response(std::string buffer) {
			return create_payload(nscp::data::command_response, buffer);
		}

		static packet create_envelope_request(unsigned long additionl_packets) {
			nscp::data::signature_packet signature;
			signature.header_length = 0;
			signature.header_type = 0;

			signature.additional_packet_count = additionl_packets;
			signature.version = nscp::data::version_1;

			std::string buffer;
			NSCPIPC::RequestEnvelope request_envelope;
			request_envelope.set_version(NSCPIPC::Common_Version_VERSION_1);
			request_envelope.set_max_supported_version(NSCPIPC::Common_Version_VERSION_1);
			request_envelope.SerializeToString(&buffer);

			signature.payload_length = buffer.size();
			signature.payload_type = nscp::data::envelope_request;

			return packet(signature, "", buffer);
		}

		static packet create_envelope_response(unsigned long additionl_packets) {
			nscp::data::signature_packet signature;
			signature.header_length = 0;
			signature.header_type = 0;

			signature.additional_packet_count = additionl_packets;
			signature.version = nscp::data::version_1;

			std::string buffer;
			NSCPIPC::RequestEnvelope request_envelope;
			request_envelope.set_version(NSCPIPC::Common_Version_VERSION_1);
			request_envelope.set_max_supported_version(NSCPIPC::Common_Version_VERSION_1);
			request_envelope.SerializeToString(&buffer);

			signature.payload_length = buffer.size();
			signature.payload_type = nscp::data::envelope_response;

			return packet(signature, "", buffer);
		}

		static packet create_error(std::wstring msg) {
			nscp::data::signature_packet signature;
			signature.header_length = 0;
			signature.header_type = 0;

			signature.additional_packet_count = 0;
			signature.version = nscp::data::version_1;

			std::string buffer;
			NSCPIPC::ErrorMessage message;
			NSCPIPC::ErrorMessage::Message *error = message.add_error();
			error->set_severity(NSCPIPC::ErrorMessage_Message_Severity_IS_ERRROR);
			error->set_message(utf8::cvt<std::string>(msg));
			message.SerializeToString(&buffer);

			signature.payload_length = buffer.size();
			signature.payload_type = nscp::data::error;

			return packet(signature, "", buffer);
		}
	};

	struct checks {
		static bool is_envelope_request(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::envelope_request;
		}
		static bool is_envelope_response(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::envelope_response;
		}
		static bool is_query_request(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::command_request;
		}
		static bool is_query_response(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::command_response;
		}
		static bool is_exec_request(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::exec_request;
		}
		static bool is_exec_response(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::exec_response;
		}
		static bool is_submit_message(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::command_response;
		}
		static bool is_error(const nscp::packet &packet) {
			return packet.signature.payload_type == nscp::data::error;
		}
	};
}

