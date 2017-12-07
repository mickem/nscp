/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/shared_ptr.hpp>

#include <socket_helpers.hpp>

using boost::asio::ip::tcp;

namespace nrpe {
	namespace client {

	class socket : public boost::noncopyable {
	private:
		boost::shared_ptr<tcp::socket> socket_;
	public:
		typedef boost::asio::basic_socket<tcp,boost::asio::stream_socket_service<tcp> >  basic_socket_type;

	public:
		socket(boost::asio::io_service &io_service, std::wstring host, int port) {
			socket_.reset(new tcp::socket(io_service));
		}
		socket() {}

		virtual boost::asio::io_service& get_io_service() {
			return socket_->get_io_service();
		}
		virtual basic_socket_type& get_socket() {
			return *socket_;
		}

		virtual void connect(std::wstring host, int port) {
			tcp::resolver resolver(get_io_service());
			tcp::resolver::query query(to_string(host), to_string(port));

			tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
			tcp::resolver::iterator end;

			boost::system::error_code error = boost::asio::error::host_not_found;
			while (error && endpoint_iterator != end) {
				tcp::resolver::endpoint_type ep = *endpoint_iterator;
				get_socket().close();
				get_socket().lowest_layer().connect(*endpoint_iterator++, error);
			}
			if (error)
				throw boost::system::system_error(error);
		}

		~socket() {
			get_socket().close();
		}

		virtual void send(nrpe::packet &packet, boost::posix_time::seconds timeout) {
			std::vector<char> buf = packet.get_buffer();
			write_with_timeout(buf, timeout);
		}
		virtual nrpe::packet recv(const nrpe::packet &packet, boost::posix_time::seconds timeout) {
			std::vector<char> buf(packet.get_packet_length());
			read_with_timeout(buf, timeout);
			return nrpe::packet(&buf[0], buf.size(), packet.get_payload_length());
		}
		virtual void read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::read_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
		virtual void write_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::write_with_timeout(*socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
	};



#ifdef USE_SSL
	class ssl_socket : public socket {
	private:
		boost::shared_ptr<boost::asio::ssl::stream<tcp::socket> > ssl_socket_;

	public:
		ssl_socket(boost::asio::io_service &io_service, boost::asio::ssl::context &ctx, std::wstring host, int port) : socket() {
			ssl_socket_.reset(new boost::asio::ssl::stream<tcp::socket>(io_service, ctx));
		}

		virtual void connect(std::wstring host, int port) {
			socket::connect(host, port);
			ssl_socket_->handshake(boost::asio::ssl::stream_base::client);
		}

		virtual boost::asio::io_service& get_io_service() {
			return ssl_socket_->get_io_service();
		}
		virtual basic_socket_type& get_socket() {
			return ssl_socket_->lowest_layer();
		}

		virtual void write_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::write_with_timeout(*ssl_socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}

		virtual void read_with_timeout(std::vector<char> &buf, boost::posix_time::seconds timeout) {
			socketHelpers::io::read_with_timeout(*ssl_socket_, get_socket(), boost::asio::buffer(buf), timeout);
		}
	};
#endif
}
}