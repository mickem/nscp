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

#include <protobuf/ipc.pb.h>
#include <protobuf/plugin.pb.h>

using namespace nscp::helpers;
//////////////////////////////////////////////////////////////////////////
//
//
// A NSCP packet consists of a number of frames.
// Each frame has a single purpose and currently two frame types are defined:
//  * Envelope frame defines a description for the sender and can be used for authentication
//  * Payload frame is the data (there can be multiple data frames)
// Each payload contains a NSCPIPC data chunk which in turn contains header information and payload but this is considered to be a application layer header.

namespace nscp {
	class data {
	public:

		static const unsigned int frame_envelope = 1;
		static const unsigned int frame_payload = 2;
		static const unsigned int frame_error = 3;

		static const unsigned int flag_last_frame = 0x1;
		static const unsigned int tag_nscp = 0x1234567890;

		struct frame_header {
			unsigned int type;
			unsigned int length;
			unsigned int flags;
			unsigned int tag;
		};
		struct frame {
			frame_header header;
			std::string payload;

			frame(unsigned int frame_type, const std::string &payload_) : payload(payload) {
				header.flags = 0;
				header.length = payload_.size();
				header.tag = tag_nscp;
				header.type = frame_type;
			}
			std::string to_string() const {
				std::stringstream ss;
				ss << "{ header { type: " << header.type << ", length: " << header.length << ", flags: " << header.flags << ", tag: " << header.tag << "}, "
					<< "payload: " << payload.size() << "}";
				return ss.str();
			}

		};

	};
	struct length {
		static std::size_t get_header_size() {
			return sizeof(data::frame_header);
		}
		static std::size_t get_frame_size(const data::frame_header &header) {
			return header.length*sizeof(char)+get_header_size();
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
		std::list<nscp::data::frame> frames_;

		packet() {}
		packet(const std::string & data) {
			add_frame(nscp::data::frame_payload, data);
		}
		packet(const packet & other) : frames_(other.frames_) {}
		const packet& operator=(const packet & other) {
			frames_ = other.frames_;
			return *this;
		}


		void add_envelope(const std::string &payload) {
			add_frame(nscp::data::frame_envelope, payload);
		}
		void add_payload(const std::string &payload) {
			add_frame(nscp::data::frame_payload, payload);
		}
		void add_frame(unsigned int frame_type, const std::string &payload) {
			nscp::data::frame frame(frame_type, payload);
			add_frame(frame);
		}
		void add_frame(nscp::data::frame frame) {
			frame.header.flags |= nscp::data::flag_last_frame;
			if (frames_.size() > 0)
				frames_.back().header.flags &= ~nscp::data::flag_last_frame;
			frames_.push_back(frame);
		}

		//////////////////////////////////////////////////////////////////////////
		// Write to string
 		std::vector<char> write() const {
 			std::vector<char> ret;
			BOOST_FOREACH(const nscp::data::frame &frame, frames_)
				write_frame(frame, ret);
 			return ret;
 		}
 		void write_frame(const nscp::data::frame &frame, std::vector<char> &buffer) const {
			const char * begin = reinterpret_cast<const char*>(&frame.header);
			const char *end = &begin[length::get_header_size()];
			buffer.insert(buffer.end(), begin, end);
			buffer.insert(buffer.end(), frame.payload.begin(), frame.payload.end());
 		}
// 		std::string write_msg_signature() const {
// 			std::string buffer;
// 			write_msg_signature(buffer);
// 			return buffer;
// 		}
// 		void write_msg_signature(std::string &buffer) const {
// 			NSCPIPC::Signature sig;
// 			sig.set_header_type(signature.header_type);
// 			sig.set_payload_type(signature.payload_type);
// 			sig.set_version(NSCPIPC::Common_Version_VERSION_1);
// 			sig.set_cookie(signature.cookie);
// 			sig.AppendToString(&buffer);
// 		}
// 		template<class T>
// 		void write_signature(T &buffer) const {
// 			nscp::data::tcp_signature_data data = signature;
// 			char * begin = reinterpret_cast<char*>(&data);
// 			char *end = &begin[length::get_signature_size()];
// 			buffer.insert(buffer.end(), begin, end);
// 		}
// 		std::string write_header() const {
// 			std::string buffer;
// 			write_header(buffer);
// 			return buffer;
// 		}
// 		template<class T>
// 		inline void write_header(T &buffer) const {
// 			if (!header.empty())
// 				buffer.insert(buffer.end(), header.begin(), header.end());
// 		}
// 		std::string write_payload() const {
// 			std::string buffer;
// 			write_payload(buffer);
// 			return buffer;
// 		}
// 		template<class T>
// 		inline void write_payload(T &buffer) const {
// 			if (!payload.empty())
// 				buffer.insert(buffer.end(), payload.begin(), payload.end());
// 		}
// 
// 		bool is_signature_valid() {
// 			return signature.magic_number == nscp::data::nscp_magic_number;
// 		}

		//////////////////////////////////////////////////////////////////////////
		// Read from vector (string?)
// 
// 		void read_msg_signature(std::string &string) {
// 			NSCPIPC::Signature sig;
// 			sig.ParseFromString(string);
// 			signature.header_length = 0;
// 			signature.payload_length = 0;
// 			signature.cookie = sig.cookie();
// 			signature.header_type = sig.header_type();
// 			signature.payload_type = sig.payload_type();
// 			signature.magic_number = nscp::data::nscp_magic_number;
// 			if (sig.version() == NSCPIPC::Common_Version_VERSION_1)
// 				signature.version = nscp::data::version_1;
// 		}
// 
// 		void read_signature(std::vector<char> &buf) {
// 			assert(buf.size() >= nscp::length::get_signature_size());
// 			read_signature(reinterpret_cast<nscp::data::tcp_signature_data*>(&(*buf.begin())));
// 		}
// 		void read_signature(std::string &string) {
// 			assert(string.size() >= nscp::length::get_signature_size());
// 			read_signature(reinterpret_cast<nscp::data::tcp_signature_data*>(&(*string.begin())));
// 		}
// 		void read_signature(std::string::iterator begin, std::string::iterator end) {
// 			assert(end-begin >= nscp::length::get_signature_size());
// 			read_signature(reinterpret_cast<nscp::data::tcp_signature_data*>(&(*begin)));
// 		}
// 		void read_signature(nscp::data::signature_type *sig) {
// 			signature = *sig;
// 		}
// 		void read_signature(nscp::data::tcp_signature_data *sig) {
// 			signature = *sig;
// 		}
// 		void nibble_signature(std::string &buf) {
// 			read_signature(buf);
// 			buf.erase(buf.begin(), buf.begin()+nscp::length::get_signature_size());
// 		}
// 		void read_header(std::string &string) {
// 			read_header(string.begin(), string.end());
// 		}
// 		void read_header(std::vector<char> &buf) {
// 			header = std::string(buf.begin(), buf.end());
// 		}
// 		void read_header(std::string::iterator begin, std::string::iterator end) {
// 			header = std::string(begin, end);
// 		}
// 		void nibble_header(std::string &buf) {
// 			assert(buf.size() >= nscp::length::get_header_size(signature));
// 			header = std::string(buf.begin(), buf.begin()+nscp::length::get_header_size(signature));
// 			buf.erase(buf.begin(), buf.begin()+nscp::length::get_header_size(signature));
// 		}
// 		void read_payload(std::string &string) {
// 			read_payload(string.begin(), string.end());
// 		}
// 		void read_payload(std::vector<char> &buf) {
// 			payload = std::string(buf.begin(), buf.end());
// 		}
// 		void read_payload(std::string::iterator begin, std::string::iterator end) {
// 			payload = std::string(begin, end);
// 		}
// 		void nibble_payload(std::string &buf) {
// 			assert(buf.size() >= nscp::length::get_payload_size(signature));
// 			payload = std::string(buf.begin(), buf.begin()+nscp::length::get_payload_size(signature));
// 			buf.erase(buf.begin(), buf.begin()+nscp::length::get_payload_size(signature));
// 		}
// 		void read_all(std::string &buffer) {
// 			std::string::iterator begin = buffer.begin();
// 			std::string::iterator end = begin+length::get_signature_size();
// 			read_signature(begin, end);
// 			begin = end;
// 			end = begin+length::get_header_size(signature);
// 			read_header(begin, end);
// 			begin = end;
// 			end = begin+length::get_payload_size(signature);
// 			read_payload(begin, end);
// 		}

		std::string to_string() const {
			std::string ret = "{";
			BOOST_FOREACH(const nscp::data::frame &f, frames_) 
				ret += "frame: " + f.to_string() + ", ";
			return ret + "}";
		}
		bool get_error(std::string &buffer);
		bool get_payload(std::string &buffer);
	};

	struct factory {

// 
// 		static nscp::data::signature_type create_simple_sig(int payload_type, std::string::size_type size) {
// 			nscp::data::signature_type signature;
// 			signature.header_length = 0;
// 			signature.header_type = 0;
// 
// 			signature.version = nscp::data::version_1;
// 
// 			signature.payload_length = size;
// 			signature.payload_type = payload_type;
// 			return signature;
// 
// 		}
// 		static nscp::data::signature_type create_sig(int payload_type, std::string::size_type header_size, std::string::size_type payload_size) {
// 			nscp::data::signature_type signature;
// 			signature.header_length = header_size;
// 			signature.header_type = 0;
// 
// 			signature.version = nscp::data::version_1;
// 
// 			signature.payload_length = payload_size;
// 			signature.payload_type = payload_type;
// 			return signature;
// 
// 		}
// 		static packet create_payload(unsigned long payload_type, std::string buffer) {
// 			nscp::data::signature_type signature;
// 			signature.header_length = 0;
// 			signature.header_type = 0;
// 
// 			signature.version = nscp::data::version_1;
// 
// 			signature.payload_length = buffer.size();
// 			signature.payload_type = payload_type;
// 
// 			return packet(signature, "", buffer);
// 		}
// 
// 		static packet create_query_response(std::string buffer) {
// 			return create_payload(nscp::data::command_response, buffer);
// 		}
// 		static packet create_submission_response(std::string buffer) {
// 			return create_payload(nscp::data::submit_response, buffer);
// 		}
// 
// 		static packet create_message_envelope_request() {
// 			std::string buffer;
// 			NSCPIPC::MessageRequestEnvelope request_envelope;
// 			request_envelope.mutable_envelope()->set_version(NSCPIPC::Common_Version_VERSION_1);
// 			request_envelope.mutable_envelope()->set_max_supported_version(NSCPIPC::Common_Version_VERSION_1);
// 			// @todo: set authentication stuff here
// 			request_envelope.SerializeToString(&buffer);
// 			return packet(create_sig(nscp::data::message_envelope_request, 0, buffer.size()), "", buffer);
// 		}
// 		static packet create_message_envelope_response(std::string cookie, int sequence) {
// 			std::string buffer;
// 			NSCPIPC::MessageResponseEnvelope envelope;
// 			envelope.mutable_envelope()->set_version(NSCPIPC::Common_Version_VERSION_1);
// 			envelope.mutable_envelope()->set_max_supported_version(NSCPIPC::Common_Version_VERSION_1);
// 			envelope.set_cookie(cookie);
// 			envelope.set_sequence(sequence);
// 			envelope.SerializeToString(&buffer);
// 			return packet(create_sig(nscp::data::message_envelope_response, 0, buffer.size()), "", buffer);
// 		}
// 
// 
// 		static packet create_envelope_request() {
// 			std::string buffer;
// 			NSCPIPC::RequestEnvelope request_envelope;
// 			request_envelope.set_version(NSCPIPC::Common_Version_VERSION_1);
// 			request_envelope.set_max_supported_version(NSCPIPC::Common_Version_VERSION_1);
// 			request_envelope.SerializeToString(&buffer);
// 			return packet(create_sig(nscp::data::envelope_request, 0, buffer.size()), "", buffer);
// 		}
// 
// 		static packet create_envelope_response() {
// 			std::string buffer;
// 			NSCPIPC::RequestEnvelope request_envelope;
// 			request_envelope.set_version(NSCPIPC::Common_Version_VERSION_1);
// 			request_envelope.set_max_supported_version(NSCPIPC::Common_Version_VERSION_1);
// 			request_envelope.SerializeToString(&buffer);
// 			return packet(create_sig(nscp::data::envelope_response, 0, buffer.size()), "", buffer);
// 		}
// 
		static packet create_error(std::string msg) {
			NSCPIPC::ErrorMessage message;
			NSCPIPC::ErrorMessage::Message *error = message.add_error();
			error->set_severity(NSCPIPC::ErrorMessage_Message_Severity_IS_ERRROR);
			error->set_message(msg);
			return packet(message.SerializeAsString());
		}
	};
// 
// 	struct checks {
// 		static bool is_envelope_request(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::envelope_request;
// 		}
// 		static bool is_envelope_response(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::envelope_response;
// 		}
// 		static bool is_message_envelope_request(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::message_envelope_request;
// 		}
// 		static bool is_message_envelope_response(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::message_envelope_response;
// 		}
// 		static bool is_query_request(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::command_request;
// 		}
// 		static bool is_query_response(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::command_response;
// 		}
// 		static bool is_exec_request(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::exec_request;
// 		}
// 		static bool is_exec_response(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::exec_response;
// 		}
// 		static bool is_submit_request(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::submit_request;
// 		}
// 		static bool is_submit_response(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::submit_response;
// 		}
// 		static bool is_error(const nscp::packet &packet) {
// 			return packet.signature.payload_type == nscp::data::error;
// 		}
// 	};
}

