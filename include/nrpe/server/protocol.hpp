#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/ssl/context.hpp>

#include <socket/socket_helpers.hpp>
#include <socket/server.hpp>


#include <string/simple.hpp>
#include "handler.hpp"
#include "parser.hpp"

namespace nrpe {
	using boost::asio::ip::tcp;

	//
	// Connection states:
	// on_accept
	// on_connect	-> connected	wants_data = true
	// on_read		-> has_more		has_data = true
	// on_write		-> has_more		has_data = true
	// on_write		-> last_packet	has_data = true
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
			has_more,
			last_packet,
			done
		};

		socket_helpers::connection_info info_;
		handler_type handler_;
		nrpe::server::parser parser_;
		state current_state_;
		outbound_buffer_type data_;
		std::list<nrpe::packet> responses_;

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

		bool on_accept(boost::asio::ip::tcp::socket& socket, int count) {
			std::list<std::string> errors;
			parser_.reset();
			std::string s = socket.remote_endpoint().address().to_string();
			if (info_.allowed_hosts.is_allowed(socket.remote_endpoint().address(), errors)) {
				log_debug(__FILE__, __LINE__, "Accepting connection from: " + s + ", count="+ss::xtos(count));
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
			return current_state_ == has_more || current_state_ == last_packet;
		}


		bool on_read(char *begin, char *end) {
			while (begin != end) {
				bool result;
				iterator_type old_begin = begin;
				boost::tie(result, begin) = parser_.digest(begin, end);
				if (result) {
					try {
						nrpe::packet request = parser_.parse();
						responses_ = handler_->handle(request);
					} catch (const std::exception &e) {
						responses_.push_back(handler_->create_error("Exception processing request: " + utf8::utf8_from_native(e.what())));
					} catch (...) {
						responses_.push_back(handler_->create_error("Exception processing request"));
					}
					queue_next();
					return true;
				} else if (begin == old_begin) {
					log_error(__FILE__, __LINE__, "Digester failed to parse chunk, giving up.");
					return false;
				}
			}
			return true;
		}
		bool has_more_response() const {
			return !responses_.empty();
		}
		void queue_next() {
			try {
				data_ = responses_.front().get_buffer();
				responses_.pop_front();
				if (has_more_response())
					set_state(has_more);
				else
					set_state(last_packet);
			} catch (const std::exception &e) {
				log_debug(__FILE__, __LINE__, "Failed to create return package: " + utf8::utf8_from_native(e.what()));
			}
		}
		void on_write() {
			if (current_state_ == last_packet)
				set_state(done);
			else
				queue_next();
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
