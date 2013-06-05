#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/ssl/context.hpp>

#include <socket/socket_helpers.hpp>
#include <socket/server.hpp>

#include "handler.hpp"
#include "parser.hpp"

namespace nrpe {
	using boost::asio::ip::tcp;

	//
	// Connection states:
	// on_accept
	// on_connect	-> connected	wants_data = true
	// on_read		-> got_req.		has_data = true
	// on_write		-> done

	static const int socket_bufer_size = 8096;
	struct read_protocol : public boost::noncopyable {
		static const bool debug_trace = false;


		typedef std::vector<char> outbound_buffer_type;
		typedef boost::shared_ptr<nrpe::server::handler> handler_type;
		typedef boost::array<char, socket_bufer_size>::iterator iterator_type;

		enum state {
			none,
			connected,
			got_request,
			done
		};

		socket_helpers::connection_info info_;
		handler_type handler_;
		nrpe::server::parser parser_;
		state current_state_;
		outbound_buffer_type data_;

		static boost::shared_ptr<read_protocol> create(socket_helpers::connection_info info, handler_type handler) {
			return boost::shared_ptr<read_protocol>(new read_protocol(info, handler));
		}

		read_protocol(socket_helpers::connection_info info, handler_type handler) 
			: info_(info)
			, handler_(handler)
			, parser_(handler->get_payload_length())
			, current_state_(none)
		{}

		inline void set_state(state new_state) {
			current_state_ = new_state;
		}

		bool on_accept(boost::asio::ip::tcp::socket& socket) {
			std::list<std::string> errors;
			parser_.reset();
			std::string s = socket.remote_endpoint().address().to_string();
			if (info_.allowed_hosts.is_allowed(socket.remote_endpoint().address(), errors)) {
				log_debug(__FILE__, __LINE__, "Accepting connection from: " + s);
				return true;
			} else {
				BOOST_FOREACH(const std::string &e, errors) {
					log_error(__FILE__, __LINE__, e);
				}
				log_error(__FILE__, __LINE__, "Rejected connection from: " + s);
				return false;
			}
		}

		bool on_connect() {
			set_state(connected);
			return true;
		}

		bool wants_data() {
			return current_state_ == connected;
		}
		bool has_data() {
			return current_state_ == got_request;
		}


		bool on_read(char *begin, char *end) {
			while (begin != end) {
				bool result;
				iterator_type old_begin = begin;
				boost::tie(result, begin) = parser_.digest(begin, end);
				if (result) {
					nrpe::packet response;
					try {
						nrpe::packet request = parser_.parse();
						response = handler_->handle(request);
					} catch (const std::exception &e) {
						response = handler_->create_error("Exception processing request: " + utf8::utf8_from_native(e.what()));
					} catch (...) {
						response = handler_->create_error("Exception processing request");
					}

					try {
						data_ = response.get_buffer();
					} catch (const std::exception &e) {
						log_debug(__FILE__, __LINE__, "Failed to create return package: " + utf8::utf8_from_native(e.what()));
					}
					set_state(got_request);
					return true;
				} else if (begin == old_begin) {
					log_error(__FILE__, __LINE__, "Digester failed to parse chunk, giving up.");
					return false;
				}
			}
			return true;
		}
		void on_write() {
			set_state(done);
		}
		std::vector<char> get_outbound() const {
			return data_;
		}

		socket_helpers::connection_info get_info() const {
			return info_;
		}

		void log_debug(std::string file, int line, std::string msg) const {
			handler_->log_debug("nrpe", file, line, msg);
		}
		void log_error(std::string file, int line, std::string msg) const {
			handler_->log_error("nrpe", file, line, msg);
		}
	};

	namespace server {
		typedef socket_helpers::server::server<read_protocol, socket_bufer_size> server;
	}

} // namespace nrpe
