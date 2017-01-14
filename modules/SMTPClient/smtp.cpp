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

#include "smtp.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/locks.hpp>
#include <boost/foreach.hpp>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>


namespace smtp {
	namespace client {
		void smtp_client::send_mail(const std::string sender, const std::list<std::string> &recipients, std::string message) {
			BOOST_FOREACH(std::string r, recipients) {
				boost::shared_ptr<envelope> en(new envelope);
				en->sender = sender;
				en->recipient = r;
				en->data = message;

				{
					boost::lock_guard<boost::mutex> lg(m);
					ready.push_front(en);
				}
			}
			async_run_queue();
		}

		void smtp_client::tick(bool) {
			size_t n = 0;
			bool active;
			{
				boost::lock_guard<boost::mutex> lg(m);
				while (!deferred.empty()) {
					ready.push_back(deferred.front());
					deferred.pop_front();
					n++;
				}
				active = !ready.empty();
			}

			if (active)
				async_run_queue();

			// 			if (n > 0) {
			// 				NSC_DEBUG_MSG(_T("activated ") + strEx::itos(n) + _T(" deferred emails"));
			// 			}
		}

		void smtp_client::async_run_queue() {
			boost::lock_guard<boost::mutex> lg(m);
			if (!active_connection) {
				active_connection.reset(new connection(shared_from_this()));
				if (active_connection)
					active_connection->start();
			}
		}

		smtp_client::connection::connection(boost::shared_ptr<smtp_client> sc)
			: sc(sc)
			, res(sc->io_service)
			, que("imap.medin.name", "1234")
			, serv(sc->io_service) {
			config["canonical-name"] = "test.medin.name";
		}

		void smtp_client::connection::start() {
			res.async_resolve(que, boost::bind(&connection::resolved, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::iterator));
		}

		void smtp_client::connection::resolved(boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator iter) {
			if (ec) {
				NSC_LOG_ERROR("smtp failed resolving: " + ec.message());
				boost::lock_guard<boost::mutex> lg(sc->m);
				sc->active_connection.reset();
				return;
			}

			if (iter == boost::asio::ip::tcp::resolver::iterator()) {
				NSC_LOG_ERROR("smtp ran out of server addresses");
				boost::lock_guard<boost::mutex> lg(sc->m);
				sc->active_connection.reset();
				return;
			}

			NSC_DEBUG_MSG("smtp connecting to " + iter->endpoint().address().to_string());
			serv.async_connect(*iter, boost::bind(&connection::connected, shared_from_this(), iter, _1));
		}

		void smtp_client::connection::connected(boost::asio::ip::tcp::resolver::iterator iter, boost::system::error_code ec) {
			if (ec) {
				NSC_LOG_ERROR("smtp failed to connect to " + iter->endpoint().address().to_string() + ": " + ec.message());
				iter++;
				resolved(boost::system::error_code(), iter);
				return;
			}

			NSC_DEBUG_MSG("smtp connected to " + iter->endpoint().address().to_string());
			state = BANNER;
			async_read_response();
		}

		void smtp_client::connection::async_read_response() {
			boost::asio::async_read_until(serv, readbuf, "\r\n", boost::bind(&connection::got_response, shared_from_this(), "", _1, _2));
		}

		void smtp_client::connection::got_response(std::string resp, boost::system::error_code ec, size_t bytes) {
			if (ec) {
				NSC_LOG_ERROR("smtp failure in reading: " + ec.message());
				boost::lock_guard<boost::mutex> lg(sc->m);
				if (cur)
					sc->ready.push_back(cur);
				sc->active_connection.reset();
				return;
			}

			std::string line;
			line.reserve(bytes);
			for (size_t i = 0; i < bytes; i++)
				line += char(readbuf.sbumpc());

			resp += line;

			if (line.length() >= 4 && line[3] == '-') {
				boost::asio::async_read_until(serv, readbuf, "\r\n", boost::bind(&connection::got_response, shared_from_this(), resp, _1, _2));
				return;
			}
			NSC_DEBUG_MSG("smtp read " + resp);

			bool broken_resp = resp.empty() || !('2' <= resp[0] && resp[0] <= '5') || (resp[0] == '3' && (state != DATA || resp.substr(0, 3) != "354"));

			// FIXME deferral / drop-on-the-floor notifications

			if (broken_resp || resp[0] == '4') {
				boost::lock_guard<boost::mutex> lg(sc->m);
				if (cur)
					sc->deferred.push_back(cur);
			}

			if (broken_resp || state == QUIT) {
				NSC_LOG_ERROR("smtp terminating");

				boost::lock_guard<boost::mutex> lg(sc->m);
				sc->active_connection.reset();
				return;
			}

			if ((resp[0] == '4' || resp[0] == '5' || state == DATA_354) && (resp.substr(0, 3) != "502" || state != EHLO)) {
				cur.reset();
				if (sc->ready.empty() || state <= RSET) {
					state = QUIT;
					send_line("QUIT");
				} else {
					state = RSET;
					send_line("RSET");
				}
				return;
			}

			assert(!resp.empty());

			switch (state) {
			case BANNER:
				assert(resp[0] == '2');
				state = EHLO;
				send_line("EHLO " + config["canonical-name"]);
				break;
			case EHLO:
				if (resp.substr(0, 3) == "502") {
					state = HELO;
					send_line("HELO" + config["canonical-name"]);
					break;
				}
				assert(resp[0] == '2');
				/* passthrough */
			case HELO:
			case RSET:
				assert(resp[0] == '2');
				assert(!cur);
				{
					boost::lock_guard<boost::mutex> lg(sc->m);
					if (sc->ready.empty()) {
						state = QUIT;
						send_line("QUIT");
						break;
					}
					cur = sc->ready.front();
					sc->ready.pop_front();
				}
				assert(cur);
				state = MAIL_FROM;
				send_line("MAIL FROM: <" + cur->sender + ">");
				break;
			case MAIL_FROM:
				assert(resp[0] == '2');
				assert(cur);
				state = RCPT_TO;
				send_line("RCPT TO: <" + cur->recipient + ">");
				break;
			case RCPT_TO:
				assert(resp[0] == '2');
				assert(cur);
				state = DATA;
				send_line("DATA");
				break;
			case DATA:
				assert(resp.substr(0, 3) == "354");
				assert(cur);
				state = DATA_354;
				send_raw(cur->data + ".\r\n");
				break;
			case DATA_354:
			case QUIT:
				assert(0); // handled above
				break;
			}
		}

		void smtp_client::connection::send_line(std::string line) {
			send_raw(line + "\r\n");
		}

		void smtp_client::connection::send_raw(std::string raw) {
			NSC_DEBUG_MSG("smtp sending " + raw);
			boost::shared_ptr<boost::asio::const_buffers_1> rb(new boost::asio::const_buffers_1(boost::asio::buffer(raw)));
			boost::asio::async_write(serv, *rb, boost::bind(&connection::sent, shared_from_this(), rb, _1, _2));
		}

		void smtp_client::connection::sent(boost::shared_ptr<boost::asio::const_buffers_1>, boost::system::error_code ec, size_t) {
			if (ec) {
				NSC_LOG_ERROR("smtp failure in reading: " + ec.message());
				boost::lock_guard<boost::mutex> lg(sc->m);
				if (cur)
					sc->ready.push_back(cur);
				sc->active_connection.reset();
				return;
			}
			async_read_response();
		}
	}
}