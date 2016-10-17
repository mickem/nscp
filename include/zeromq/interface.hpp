#pragma once

#include <zmq.hpp>
#include <nscp/handler.hpp>

namespace zeromq {

	struct zeromq_reader_interface {
		virtual bool send(boost::shared_ptr<zmq::socket_t> socket) = 0;
		virtual bool read(boost::shared_ptr<zmq::socket_t> socket) = 0;
		virtual bool send_wrapped(boost::shared_ptr<zmq::socket_t> socket) = 0;
		virtual bool read_wrapped(boost::shared_ptr<zmq::socket_t> socket) = 0;
		virtual void error(std::string problem) = 0;
	};

	struct zeromq_reader_impl : public zeromq_reader_interface {
		bool send_chunk(boost::shared_ptr<zmq::socket_t> socket, std::string buffer, bool more = false) {
			NSC_DEBUG_MSG(_T("--->") + to_wstring(strEx::format_buffer(buffer)));
			zxmsg msg(buffer);
			if (!socket) {
				error("No socket: cant send...");
				return false;
			}
			return socket->send(msg, more?ZMQ_SNDMORE:0);
		}
		bool read_chunk(boost::shared_ptr<zmq::socket_t> socket, std::string &buffer) {
			zmq::message_t msg;
			if (!socket) {
				error("No socket: cant read...");
				return false;
			}
			if (!socket->recv(&msg))
				return false;
			buffer = std::string(reinterpret_cast<char*>(msg.data()), msg.size());
			NSC_DEBUG_MSG(_T("<---") + to_wstring(strEx::format_buffer(buffer)));
			return true;
		}
	};


	struct zeromq_reader : public zeromq_reader_impl {
		virtual nscp::packet get_outbound() = 0;
		virtual bool process_inbound(nscp::packet &packet) = 0;
		std::string address;

		inline bool err_io(std::string type) {
			error("Failed to read/write: " + type);
			return false;
		}

		bool send_wrapped(boost::shared_ptr<zmq::socket_t> socket) {
			std::string empty;
			if (!send_chunk(socket, address, true))
				return err_io("address");
			if (!send_chunk(socket, empty, true))
				return err_io("separator");
			return send(socket);
		}
		bool send(boost::shared_ptr<zmq::socket_t> socket) {
			nscp::packet request = get_outbound();
			NSC_DEBUG_MSG(_T("(--->sig::signature)") + to_wstring(request.to_wstring()));
			if (!send_chunk(socket, request.write_msg_signature(), true))
				return err_io("signature");
			if (!send_chunk(socket, request.write_header(), true))
				return err_io("header");
			if (!send_chunk(socket, request.write_payload()))
				return err_io("payload");
			return true;
		}
		bool read_wrapped(boost::shared_ptr<zmq::socket_t> socket) {
			std::string empty;
			if (!read_chunk(socket, address))
				return err_io("address");
			if (!read_chunk(socket, empty))
				return err_io("separator");
			return read(socket);
		}
		bool read(boost::shared_ptr<zmq::socket_t> socket) {
			nscp::packet response;
			std::string signature, header, payload;
			if (!read_chunk(socket, signature))
				return err_io("signature");
			if (!read_chunk(socket, header))
				return err_io("header");
			if (!read_chunk(socket, payload))
				return err_io("payload");
			response.read_msg_signature(signature);
			NSC_DEBUG_MSG(_T("(<---sig::signature)") + to_wstring(response.to_wstring()));
			if (!response.is_signature_valid()) {
				error("Got invalid signature from client (ignoring)");
				return false;
			}
			response.read_header(header);
			response.read_payload(payload);
			return process_inbound(response);
		}
	};


}