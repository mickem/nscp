// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <net/http/http_client_protocol.hpp>

TEST(http_client_protocol, initial_state_has_no_data_and_wants_no_data) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

TEST(http_client_protocol, prepare_request_exposes_outbound_data) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::request req("GET", "example.com", "/test");

  proto.prepare_request(req);

  EXPECT_TRUE(proto.has_data());
  EXPECT_FALSE(proto.get_outbound().empty());
}

TEST(http_client_protocol, on_write_switches_to_read_state) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::request req("GET", "example.com", "/test");
  proto.prepare_request(req);

  EXPECT_FALSE(proto.on_write(0));
  EXPECT_TRUE(proto.wants_data());
}

TEST(http_client_protocol, on_read_in_read_state_collects_response_data) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::request req("GET", "example.com", "/test");
  proto.prepare_request(req);
  proto.on_write(0);

  const std::string raw = "HTTP/1.1 200\r\nContent-Type: text/plain\r\n\r\nhello";
  proto.get_inbound().assign(raw.begin(), raw.end());

  EXPECT_TRUE(proto.on_read(raw.size()));
  const http::response response = proto.get_response();
  EXPECT_EQ(response.status_code_, 200u);
  EXPECT_EQ(response.payload_, "hello");
}

// The test above sends a status line with no reason phrase, which no real
// server does. Parsing "HTTP/1.1 200 OK" used to throw bad_lexical_cast out of
// the response constructor, so every reply from a real server failed here -
// this is the path NSCPClient reads a remote agent's check result from.
TEST(http_client_protocol, reads_a_status_line_with_a_reason_phrase) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::request req("GET", "example.com", "/test");
  proto.prepare_request(req);
  proto.on_write(0);

  const std::string raw = "HTTP/1.1 202 Accepted\r\nContent-Type: text/plain\r\n\r\nWARNING: warm|'load'=1";
  proto.get_inbound().assign(raw.begin(), raw.end());

  EXPECT_TRUE(proto.on_read(raw.size()));
  const http::response response = proto.get_response();
  EXPECT_EQ(response.status_code_, 202u);
  EXPECT_EQ(response.status_message_, "Accepted");
  EXPECT_EQ(response.payload_, "WARNING: warm|'load'=1");
}

// get_inbound() and get_outbound() are the same vector, still holding the
// request that was just sent. A reply shorter than that request leaves our own
// bytes in the tail, and on_read used to append the whole buffer regardless of
// how much was actually read - so a short 403 came back with "Connection:
// close" and the password header we sent stuck to its body. Simulated here the
// way the socket layer does it: the buffer keeps its request-sized length, the
// reply is written over the front, and on_read is told how many bytes landed.
TEST(http_client_protocol, a_short_reply_does_not_carry_the_tail_of_our_request) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::request req("GET", "example.com", "/test");
  proto.prepare_request(req);
  const std::size_t request_size = proto.get_outbound().size();
  proto.on_write(0);

  const std::string reply = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nnope";
  ASSERT_LT(reply.size(), request_size) << "the request must be the longer of the two for this to bite";
  std::copy(reply.begin(), reply.end(), proto.get_inbound().begin());

  EXPECT_TRUE(proto.on_read(reply.size()));
  const http::response response = proto.get_response();
  EXPECT_EQ(response.status_code_, 403u);
  EXPECT_EQ(response.payload_, "nope");
  EXPECT_EQ(response.payload_.find("password"), std::string::npos);
  EXPECT_EQ(response.payload_.find("Connection"), std::string::npos);
}

// The same buffer reuse repeated earlier bytes when a reply arrived in several
// reads, because every on_read appended the whole buffer rather than its own.
TEST(http_client_protocol, a_reply_split_across_reads_is_assembled_once) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::request req("GET", "example.com", "/test");
  proto.prepare_request(req);
  proto.on_write(0);

  const std::string first = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nfir";
  const std::string second = "st-and-second";
  std::copy(first.begin(), first.end(), proto.get_inbound().begin());
  EXPECT_TRUE(proto.on_read(first.size()));
  std::copy(second.begin(), second.end(), proto.get_inbound().begin());
  EXPECT_TRUE(proto.on_read(second.size()));

  const http::response response = proto.get_response();
  EXPECT_EQ(response.status_code_, 200u);
  EXPECT_EQ(response.payload_, "first-and-second");
}

TEST(http_client_protocol, on_read_when_not_waiting_marks_done) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  proto.get_inbound().assign({'x'});
  EXPECT_TRUE(proto.on_read(1));
  EXPECT_FALSE(proto.has_data());
  EXPECT_FALSE(proto.wants_data());
}

TEST(http_client_protocol, on_read_error_in_read_state_finishes_successfully) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);
  http::request req("GET", "example.com", "/test");
  proto.prepare_request(req);
  proto.on_write(0);

  boost::system::error_code ec;
  EXPECT_TRUE(proto.on_read_error(ec));
  EXPECT_FALSE(proto.wants_data());
}

TEST(http_client_protocol, on_read_error_before_read_state_fails) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  boost::system::error_code ec;
  EXPECT_FALSE(proto.on_read_error(ec));
}

TEST(http_client_protocol, timeout_response_has_status_99_and_message) {
  std::shared_ptr<http::client::protocol::client_handler> handler;
  http::client::protocol proto(handler);

  const http::response timeout = proto.get_timeout_response();
  EXPECT_EQ(timeout.status_code_, 99);
  EXPECT_EQ(timeout.payload_, "Failed to read data");
}
