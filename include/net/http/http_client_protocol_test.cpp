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

#include <gtest/gtest.h>

#include <net/http/http_client_protocol.hpp>

TEST(http_client_protocol, initial_state_has_no_data_and_wants_no_data) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

TEST(http_client_protocol, prepare_request_exposes_outbound_data) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::packet req("GET", "example.com", "/test");

  proto.prepare_request(req);

  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.get_outbound().empty());
}

TEST(http_client_protocol, on_write_switches_to_read_state) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::packet req("GET", "example.com", "/test");
  proto.prepare_request(req);

  EXPECT_FALSE(proto.on_write(0));
  EXPECT_TRUE(proto.wants_data());
}

TEST(http_client_protocol, on_read_in_read_state_collects_response_data) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::packet req("GET", "example.com", "/test");
  proto.prepare_request(req);
  proto.on_write(0);

  const std::string raw = "HTTP/1.1 200\r\nContent-Type: text/plain\r\n\r\nhello";
  proto.get_inbound().assign(raw.begin(), raw.end());

  EXPECT_TRUE(proto.on_read(raw.size()));
  const http::packet response = proto.get_response();
  EXPECT_EQ(response.status_code_, 200);
  EXPECT_EQ(response.payload_, "hello");
}

TEST(http_client_protocol, on_read_when_not_waiting_marks_done) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  proto.get_inbound().assign({'x'});
  EXPECT_TRUE(proto.on_read(1));
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

TEST(http_client_protocol, on_read_error_in_read_state_finishes_successfully) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::packet req("GET", "example.com", "/test");
  proto.prepare_request(req);
  proto.on_write(0);

  boost::system::error_code ec;
  EXPECT_TRUE(proto.on_read_error(ec));
  EXPECT_FALSE(proto.wants_data());
}

TEST(http_client_protocol, on_read_error_before_read_state_fails) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  boost::system::error_code ec;
  EXPECT_FALSE(proto.on_read_error(ec));
}

TEST(http_client_protocol, timeout_response_has_status_99_and_message) {
  boost::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  const http::packet timeout = proto.get_timeout_response();
  EXPECT_EQ(timeout.status_code_, 99);
  EXPECT_EQ(timeout.payload_, "Failed to read data");
}
