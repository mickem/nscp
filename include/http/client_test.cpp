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

#include <http/client.hpp>
#include <sstream>
#include <string>

// =============================================================================
// http_client_options tests
// =============================================================================

TEST(http_client_options, is_https_true) {
  const http::http_client_options opts("https", "1.2", "peer", "ca.pem");
  EXPECT_TRUE(opts.is_https());
}

TEST(http_client_options, is_https_false_for_http) {
  const http::http_client_options opts("http", "1.2", "peer", "");
  EXPECT_FALSE(opts.is_https());
}

TEST(http_client_options, is_pipe_true) {
  const http::http_client_options opts("pipe", "", "", "");
  EXPECT_TRUE(opts.is_pipe());
}

TEST(http_client_options, is_pipe_false_for_http) {
  const http::http_client_options opts("http", "", "", "");
  EXPECT_FALSE(opts.is_pipe());
}

TEST(http_client_options, is_pipe_false_for_https) {
  const http::http_client_options opts("https", "1.2", "peer", "");
  EXPECT_FALSE(opts.is_pipe());
}

TEST(http_client_options, stores_protocol) {
  const http::http_client_options opts("https", "1.2", "peer", "ca.pem");
  EXPECT_EQ(opts.protocol_, "https");
}

TEST(http_client_options, stores_tls_version) {
  const http::http_client_options opts("https", "1.2+", "peer", "ca.pem");
  EXPECT_EQ(opts.tls_version_, "1.2+");
}

TEST(http_client_options, stores_verify) {
  const http::http_client_options opts("https", "1.2", "none", "");
  EXPECT_EQ(opts.verify_, "none");
}

TEST(http_client_options, stores_ca) {
  const http::http_client_options opts("https", "1.2", "peer", "/path/to/ca.pem");
  EXPECT_EQ(opts.ca_, "/path/to/ca.pem");
}

// =============================================================================
// http::packet tests
// =============================================================================

TEST(http_packet, default_constructor) {
  const http::packet p;
  EXPECT_TRUE(p.verb_.empty());
  EXPECT_TRUE(p.server_.empty());
  EXPECT_TRUE(p.path_.empty());
  EXPECT_TRUE(p.payload_.empty());
}

TEST(http_packet, constructor_with_verb_server_path) {
  const http::packet p("GET", "example.com", "/api/v1");
  EXPECT_EQ(p.verb_, "GET");
  EXPECT_EQ(p.server_, "example.com");
  EXPECT_EQ(p.path_, "/api/v1");
  EXPECT_TRUE(p.payload_.empty());
  EXPECT_EQ(p.status_code_, 0);
}

TEST(http_packet, constructor_with_payload) {
  const http::packet p("POST", "example.com", "/api/v1", "body data");
  EXPECT_EQ(p.verb_, "POST");
  EXPECT_EQ(p.server_, "example.com");
  EXPECT_EQ(p.path_, "/api/v1");
  EXPECT_EQ(p.payload_, "body data");
}

TEST(http_packet, add_default_headers) {
  http::packet p("GET", "example.com", "/");
  // add_default_headers is called by constructor
  EXPECT_EQ(p.headers_["Accept"], "*/*");
  EXPECT_EQ(p.headers_["Connection"], "close");
}

TEST(http_packet, add_header_key_value) {
  http::packet p;
  p.add_header("Content-Type", "application/json");
  EXPECT_EQ(p.headers_["Content-Type"], "application/json");
}

TEST(http_packet, add_header_from_line) {
  http::packet p;
  p.add_header("Content-Type: application/json");
  EXPECT_EQ(p.headers_["Content-Type"], " application/json");
}

TEST(http_packet, add_header_from_line_no_colon) {
  http::packet p;
  p.add_header("InvalidHeader");
  EXPECT_EQ(p.headers_["InvalidHeader"], "");
}

TEST(http_packet, set_path) {
  http::packet p;
  p.set_path("PUT", "/new/path");
  EXPECT_EQ(p.verb_, "PUT");
  EXPECT_EQ(p.path_, "/new/path");
}

TEST(http_packet, set_payload) {
  http::packet p;
  p.set_payload("test payload");
  EXPECT_EQ(p.payload_, "test payload");
}

TEST(http_packet, get_header_contains_verb_and_path) {
  const http::packet p("GET", "example.com", "/test");
  const std::string header = p.get_header();
  EXPECT_NE(header.find("GET /test HTTP/1.0\r\n"), std::string::npos);
  EXPECT_NE(header.find("Host: example.com\r\n"), std::string::npos);
}

TEST(http_packet, get_header_no_host_when_server_empty) {
  http::packet p;
  p.verb_ = "GET";
  p.path_ = "/test";
  const std::string header = p.get_header();
  EXPECT_EQ(header.find("Host:"), std::string::npos);
}

TEST(http_packet, get_payload_empty) {
  const http::packet p;
  EXPECT_EQ(p.get_payload(), "");
}

TEST(http_packet, get_payload_with_data) {
  http::packet p;
  p.set_payload("some data");
  EXPECT_EQ(p.get_payload(), "some data");
}

TEST(http_packet, get_packet_combines_header_and_payload) {
  const http::packet p("GET", "example.com", "/test", "payload");
  const std::vector<char> pkt = p.get_packet();
  const std::string pkt_str(pkt.begin(), pkt.end());
  EXPECT_NE(pkt_str.find("GET /test HTTP/1.0"), std::string::npos);
  EXPECT_NE(pkt_str.find("payload"), std::string::npos);
}

TEST(http_packet, build_request_output) {
  http::packet p("POST", "example.com", "/api");
  p.set_payload("body");
  std::ostringstream os;
  p.build_request(os);
  const std::string result = os.str();
  EXPECT_NE(result.find("POST /api HTTP/1.0\r\n"), std::string::npos);
  EXPECT_NE(result.find("Host: example.com\r\n"), std::string::npos);
  EXPECT_NE(result.find("body"), std::string::npos);
}

TEST(http_packet, create_timeout) {
  const http::packet p = http::packet::create_timeout("timed out");
  EXPECT_EQ(p.status_code_, 99);
  EXPECT_EQ(p.payload_, "timed out");
}

TEST(http_packet, to_string_output) {
  const http::packet p("GET", "example.com", "/path");
  const std::string s = p.to_string();
  EXPECT_NE(s.find("verb: GET"), std::string::npos);
  EXPECT_NE(s.find("path: /path"), std::string::npos);
}

TEST(http_packet, add_post_payload_map) {
  http::packet p("GET", "example.com", "/");
  http::packet::post_map_type payload;
  payload["key1"] = "value1";
  payload["key2"] = "value 2";
  p.add_post_payload(payload);
  EXPECT_EQ(p.verb_, "POST");
  EXPECT_NE(p.headers_.find("Content-Length"), p.headers_.end());
  EXPECT_EQ(p.headers_["Content-Type"], "application/x-www-form-urlencoded");
  EXPECT_NE(p.payload_.find("key1=value1"), std::string::npos);
  EXPECT_NE(p.payload_.find("key2=value+2"), std::string::npos);
}

TEST(http_packet, add_post_payload_string) {
  http::packet p("GET", "example.com", "/");
  p.add_post_payload("application/json", "{\"key\":\"value\"}");
  EXPECT_EQ(p.verb_, "POST");
  EXPECT_EQ(p.headers_["Content-Type"], "application/json");
  EXPECT_EQ(p.payload_, "{\"key\":\"value\"}");
  EXPECT_EQ(p.headers_["Content-Length"], std::to_string(std::string("{\"key\":\"value\"}").size()));
}

TEST(http_packet, parse_http_response) {
  http::packet p;
  p.parse_http_response("HTTP/1.1 200");
  EXPECT_EQ(p.status_code_, 200);
}

TEST(http_packet, parse_http_response_no_space) {
  http::packet p;
  p.parse_http_response("HTTP/1.1");
  EXPECT_EQ(p.status_code_, 500);
}

// =============================================================================
// http::response tests
// =============================================================================

TEST(http_response, default_constructor) {
  const http::response r;
  EXPECT_TRUE(r.http_version_.empty());
  EXPECT_TRUE(r.status_message_.empty());
  EXPECT_TRUE(r.payload_.empty());
}

TEST(http_response, parameterized_constructor) {
  const http::response r("HTTP/1.1", 200, "OK");
  EXPECT_EQ(r.http_version_, "HTTP/1.1");
  EXPECT_EQ(r.status_code_, 200u);
  EXPECT_EQ(r.status_message_, "OK");
}

TEST(http_response, copy_constructor) {
  http::response r1("HTTP/1.1", 200, "OK");
  r1.payload_ = "body";
  r1.add_header("Content-Type", "text/html");
  http::response r2(r1);
  EXPECT_EQ(r2.http_version_, "HTTP/1.1");
  EXPECT_EQ(r2.status_code_, 200u);
  EXPECT_EQ(r2.status_message_, "OK");
  EXPECT_EQ(r2.payload_, "body");
  EXPECT_EQ(r2.headers_["Content-Type"], "text/html");
}

TEST(http_response, assignment_operator) {
  http::response r1("HTTP/1.1", 201, "Created");
  r1.payload_ = "created body";
  http::response r2;
  r2 = r1;
  EXPECT_EQ(r2.http_version_, "HTTP/1.1");
  EXPECT_EQ(r2.status_code_, 201u);
  EXPECT_EQ(r2.status_message_, "Created");
  EXPECT_EQ(r2.payload_, "created body");
}

TEST(http_response, is_2xx_true_for_200) {
  const http::response r("HTTP/1.1", 200, "OK");
  EXPECT_TRUE(r.is_2xx());
}

TEST(http_response, is_2xx_true_for_201) {
  const http::response r("HTTP/1.1", 201, "Created");
  EXPECT_TRUE(r.is_2xx());
}

TEST(http_response, is_2xx_true_for_299) {
  const http::response r("HTTP/1.1", 299, "Custom");
  EXPECT_TRUE(r.is_2xx());
}

TEST(http_response, is_2xx_false_for_300) {
  const http::response r("HTTP/1.1", 300, "Redirect");
  EXPECT_FALSE(r.is_2xx());
}

TEST(http_response, is_2xx_false_for_404) {
  const http::response r("HTTP/1.1", 404, "Not Found");
  EXPECT_FALSE(r.is_2xx());
}

TEST(http_response, is_2xx_false_for_500) {
  const http::response r("HTTP/1.1", 500, "Internal Server Error");
  EXPECT_FALSE(r.is_2xx());
}

TEST(http_response, is_2xx_false_for_199) {
  const http::response r("HTTP/1.1", 199, "Info");
  EXPECT_FALSE(r.is_2xx());
}

TEST(http_response, add_header_key_value) {
  http::response r;
  r.add_header("X-Custom", "value");
  EXPECT_EQ(r.headers_["X-Custom"], "value");
}

TEST(http_response, add_header_from_line) {
  http::response r;
  r.add_header("Content-Type: text/plain");
  EXPECT_EQ(r.headers_["Content-Type"], " text/plain");
}

TEST(http_response, add_header_from_line_no_colon) {
  http::response r;
  r.add_header("NoColonHeader");
  EXPECT_EQ(r.headers_["NoColonHeader"], "");
}

// =============================================================================
// uri_encode tests
// =============================================================================

TEST(uri_encode, encodes_space_as_plus) {
  EXPECT_EQ(http::uri_encode("hello world"), "hello+world");
}

TEST(uri_encode, leaves_alphanumeric_unchanged) {
  EXPECT_EQ(http::uri_encode("abcXYZ012"), "abcXYZ012");
}

TEST(uri_encode, encodes_special_characters) {
  const std::string result = http::uri_encode("@#$");
  EXPECT_EQ(result, "%40%23%24");
}

TEST(uri_encode, preserves_mark_characters) {
  EXPECT_EQ(http::uri_encode("-_.!~*'()"), "-_.!~*'()");
}

TEST(uri_encode, empty_string) {
  EXPECT_EQ(http::uri_encode(""), "");
}

TEST(uri_encode, encodes_slash) {
  EXPECT_EQ(http::uri_encode("/"), "%2F");
}

// =============================================================================
// simple_client constructor tests (socket type selection)
// =============================================================================

TEST(simple_client, constructor_http_creates_client) {
  const http::http_client_options opts("http", "", "", "");
  EXPECT_NO_THROW(const http::simple_client client(opts));
}

TEST(simple_client, constructor_https_creates_client) {
  const http::http_client_options opts("https", "1.2", "none", "");
  EXPECT_NO_THROW(const http::simple_client client(opts));
}

#ifdef WIN32
TEST(simple_client, constructor_pipe_creates_client) {
  const http::http_client_options opts("pipe", "", "", "");
  EXPECT_NO_THROW(const http::simple_client client(opts));
}
#endif

// =============================================================================
// simple_client request building test
// =============================================================================

TEST(simple_client, send_request_builds_correct_http) {
  const http::packet rq("GET", "example.com", "/test");
  std::ostringstream os;
  rq.build_request(os);
  const std::string request = os.str();
  EXPECT_NE(request.find("GET /test HTTP/1.0"), std::string::npos);
  EXPECT_NE(request.find("Host: example.com"), std::string::npos);
  EXPECT_NE(request.find("Accept: */*"), std::string::npos);
  EXPECT_NE(request.find("Connection: close"), std::string::npos);
}

// =============================================================================
// Packet construction from raw data (vector<char> constructor)
// =============================================================================

TEST(http_packet, construct_from_raw_data) {
  const std::string raw = "HTTP/1.1 200\r\nContent-Type: text/html\r\n\r\nHello Body";
  const std::vector<char> data(raw.begin(), raw.end());
  http::packet p(data);
  EXPECT_EQ(p.status_code_, 200);
  EXPECT_EQ(p.headers_["Content-Type"], " text/html");
  EXPECT_EQ(p.payload_, "Hello Body");
}

TEST(http_packet, construct_from_raw_data_no_body) {
  const std::string raw = "HTTP/1.1 404\r\nX-Custom: val\r\n\r\n";
  const std::vector<char> data(raw.begin(), raw.end());
  const http::packet p(data);
  EXPECT_EQ(p.status_code_, 404);
}

// =============================================================================
// download static method - connection failure test
// =============================================================================

TEST(simple_client, download_fails_with_bad_host) {
  std::ostringstream os;
  std::string error_msg;
  const bool result = http::simple_client::download("http", "this-host-does-not-exist.invalid", "80", "/", "", "", "", os, error_msg);
  EXPECT_FALSE(result);
  EXPECT_FALSE(error_msg.empty());
}

// =============================================================================
// socket_exception tests
// =============================================================================

TEST(socket_exception, reason_returns_message) {
  const socket_helpers::socket_exception ex("test error");
  EXPECT_EQ(ex.reason(), "test error");
  EXPECT_STREQ(ex.what(), "test error");
}

TEST(socket_exception, copy_constructor) {
  const socket_helpers::socket_exception ex1("original");
  const socket_helpers::socket_exception ex2(ex1);
  EXPECT_EQ(ex2.reason(), "original");
}

