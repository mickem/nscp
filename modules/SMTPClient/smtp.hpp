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

#include <list>
#include <string>

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>

namespace smtp {
	namespace client {
		class smtp_client : private boost::noncopyable, public boost::enable_shared_from_this<smtp_client> {
			struct envelope {
				std::string sender;
				std::string recipient;
				std::string data;
			};

			class connection : private boost::noncopyable, public boost::enable_shared_from_this<connection> {
			public:
				//typedef boost::shared_ptr<connection> ptr;

				connection(boost::shared_ptr<smtp_client>);
				void start();
				std::map<std::string, std::string> config;
			private:
				boost::shared_ptr<smtp_client> sc;

				boost::asio::ip::tcp::resolver res;
				boost::asio::ip::tcp::resolver::query que;
				boost::asio::ip::tcp::socket serv;

				enum state_type {
					BANNER,
					EHLO,
					HELO,
					RSET,
					MAIL_FROM,
					RCPT_TO,
					DATA,
					DATA_354,
					QUIT
				} state;
				boost::shared_ptr<envelope> cur;

				boost::asio::streambuf readbuf;

				void resolved(boost::system::error_code, boost::asio::ip::tcp::resolver::iterator);
				void connected(boost::asio::ip::tcp::resolver::iterator, boost::system::error_code ec);
				void async_read_response();
				void got_response(std::string resp, boost::system::error_code ec, size_t bytes);
				void send_line(std::string line);
				void send_raw(std::string raw);
				void sent(boost::shared_ptr<boost::asio::const_buffers_1>, boost::system::error_code ec, size_t);
			};

			boost::asio::io_service &io_service;
			boost::shared_ptr<connection> active_connection;

			boost::mutex m;
			std::list<boost::shared_ptr<envelope> > ready;
			std::list<boost::shared_ptr<envelope> > deferred;

			void async_run_queue();
		public:
			smtp_client(boost::asio::io_service &io_service) : io_service(io_service) {}

			void send_mail(const std::string sender, const std::list<std::string> &recipients, std::string message);
			void tick(bool);
		};
	}
}