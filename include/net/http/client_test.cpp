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

#include <boost/asio.hpp>
#include <future>
#include <net/http/client.hpp>
#include <sstream>
#include <string>
#include <thread>

namespace {

class loopback_http_server {
 public:
  explicit loopback_http_server(std::string response) : response_(std::move(response)), port_(0) {
    std::promise<unsigned short> port_promise;
    std::future<unsigned short> port_future = port_promise.get_future();
    thread_ = std::thread([this, p = std::move(port_promise)]() mutable {
      try {
        boost::asio::io_context io;
        boost::asio::ip::tcp::acceptor acceptor(io, {boost::asio::ip::tcp::v4(), 0});
        p.set_value(acceptor.local_endpoint().port());

        boost::asio::ip::tcp::socket socket(io);
        acceptor.accept(socket);

        boost::asio::streambuf request;
        boost::asio::read_until(socket, request, "\r\n\r\n");
        captured_request_.assign(std::istreambuf_iterator<char>(&request), std::istreambuf_iterator<char>());

        boost::asio::write(socket, boost::asio::buffer(response_));
      } catch (...) {
        try {
          p.set_exception(std::current_exception());
        } catch (...) {
        }
      }
    });
    port_ = port_future.get();
  }

  ~loopback_http_server() {
    if (thread_.joinable()) thread_.join();
  }

  unsigned short port() const { return port_; }
  const std::string &captured_request() const { return captured_request_; }

 private:
  std::string response_;
  unsigned short port_;
  std::string captured_request_;
  std::thread thread_;
};

}  // namespace

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

TEST(uri_encode, encodes_space_as_plus) { EXPECT_EQ(http::uri_encode("hello world"), "hello+world"); }

TEST(uri_encode, leaves_alphanumeric_unchanged) { EXPECT_EQ(http::uri_encode("abcXYZ012"), "abcXYZ012"); }

TEST(uri_encode, encodes_special_characters) {
  const std::string result = http::uri_encode("@#$");
  EXPECT_EQ(result, "%40%23%24");
}

TEST(uri_encode, preserves_mark_characters) { EXPECT_EQ(http::uri_encode("-_.!~*'()"), "-_.!~*'()"); }

TEST(uri_encode, empty_string) { EXPECT_EQ(http::uri_encode(""), ""); }

TEST(uri_encode, encodes_slash) { EXPECT_EQ(http::uri_encode("/"), "%2F"); }

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

// =============================================================================
// base64_encode (used for Proxy-Authorization)
// =============================================================================

TEST(base64_encode, empty_string) { EXPECT_EQ(http::base64_encode(""), ""); }

TEST(base64_encode, single_char_padded) {
  // 'A' = 0x41 → base64 "QQ=="
  EXPECT_EQ(http::base64_encode("A"), "QQ==");
}

TEST(base64_encode, two_chars_padded) {
  // "AB" → "QUI="
  EXPECT_EQ(http::base64_encode("AB"), "QUI=");
}

TEST(base64_encode, three_chars_no_padding) {
  // "ABC" → "QUJD"
  EXPECT_EQ(http::base64_encode("ABC"), "QUJD");
}

TEST(base64_encode, well_known_encoding) {
  // RFC 4648 test vector: "Man" → "TWFu"
  EXPECT_EQ(http::base64_encode("Man"), "TWFu");
}

TEST(base64_encode, credential_string) {
  // "user:pass" → "dXNlcjpwYXNz"
  EXPECT_EQ(http::base64_encode("user:pass"), "dXNlcjpwYXNz");
}

// =============================================================================
// simple_client::make_proxy_request — HTTP proxy request rewriting
// =============================================================================

TEST(make_proxy_request, rewrites_path_to_absolute_uri) {
  const http::packet original("GET", "target.corp", "/api/v1");
  http::proxy_config proxy;
  proxy.type = http::proxy_type::HTTP;
  proxy.host = "proxy.corp";
  proxy.port = "3128";

  const http::packet result = http::simple_client::make_proxy_request(original, "target.corp", "8080", proxy);
  EXPECT_EQ(result.path_, "http://target.corp:8080/api/v1");
}

TEST(make_proxy_request, omits_port_80_from_absolute_uri) {
  const http::packet original("GET", "target.corp", "/path");
  http::proxy_config proxy;
  proxy.type = http::proxy_type::HTTP;
  proxy.host = "proxy.corp";
  proxy.port = "3128";

  const http::packet result = http::simple_client::make_proxy_request(original, "target.corp", "80", proxy);
  EXPECT_EQ(result.path_, "http://target.corp/path");
}

TEST(make_proxy_request, adds_proxy_authorization_when_credentials_present) {
  const http::packet original("GET", "target.corp", "/path");
  http::proxy_config proxy;
  proxy.type = http::proxy_type::HTTP;
  proxy.host = "proxy.corp";
  proxy.port = "3128";
  proxy.username = "alice";
  proxy.password = "secret";

  const http::packet result = http::simple_client::make_proxy_request(original, "target.corp", "80", proxy);
  const auto it = result.headers_.find("Proxy-Authorization");
  ASSERT_NE(it, result.headers_.end());
  // "alice:secret" in base64 is "YWxpY2U6c2VjcmV0"
  EXPECT_EQ(it->second, "Basic YWxpY2U6c2VjcmV0");
}

TEST(make_proxy_request, no_proxy_authorization_without_credentials) {
  const http::packet original("GET", "target.corp", "/path");
  http::proxy_config proxy;
  proxy.type = http::proxy_type::HTTP;
  proxy.host = "proxy.corp";
  proxy.port = "3128";

  const http::packet result = http::simple_client::make_proxy_request(original, "target.corp", "80", proxy);
  EXPECT_EQ(result.headers_.count("Proxy-Authorization"), 0u);
}

TEST(make_proxy_request, preserves_original_headers) {
  http::packet original("GET", "target.corp", "/path");
  original.add_header("X-Custom", "value");
  http::proxy_config proxy;
  proxy.type = http::proxy_type::HTTP;
  proxy.host = "proxy.corp";
  proxy.port = "3128";

  const http::packet result = http::simple_client::make_proxy_request(original, "target.corp", "80", proxy);
  const auto it = result.headers_.find("X-Custom");
  ASSERT_NE(it, result.headers_.end());
  EXPECT_EQ(it->second, "value");
}

TEST(make_proxy_request, build_request_contains_absolute_uri_in_request_line) {
  const http::packet original("GET", "target.corp", "/api/v1");
  http::proxy_config proxy;
  proxy.type = http::proxy_type::HTTP;
  proxy.host = "proxy.corp";
  proxy.port = "3128";

  const http::packet result = http::simple_client::make_proxy_request(original, "target.corp", "8080", proxy);
  std::ostringstream os;
  result.build_request(os);
  const std::string built = os.str();
  EXPECT_NE(built.find("GET http://target.corp:8080/api/v1 HTTP/1.0"), std::string::npos);
}

// =============================================================================
// http_client_options — proxy field
// =============================================================================

TEST(http_client_options, proxy_defaults_to_none_when_not_provided) {
  const http::http_client_options opts("http", "", "", "");
  EXPECT_EQ(opts.proxy_.type, http::proxy_type::NONE);
  EXPECT_FALSE(opts.proxy_.is_set());
}

TEST(http_client_options, proxy_stored_when_provided) {
  http::proxy_config proxy;
  proxy.type = http::proxy_type::HTTP;
  proxy.host = "proxy.corp";
  proxy.port = "3128";
  const http::http_client_options opts("http", "", "", "", proxy);
  EXPECT_EQ(opts.proxy_.type, http::proxy_type::HTTP);
  EXPECT_EQ(opts.proxy_.host, "proxy.corp");
  EXPECT_EQ(opts.proxy_.port, "3128");
}

// =============================================================================
// parse_url tests
// =============================================================================

TEST(parse_url, missing_scheme_returns_empty_result) {
  const http::parsed_url parsed = http::parse_url("example.com/path");
  EXPECT_TRUE(parsed.protocol.empty());
  EXPECT_TRUE(parsed.host.empty());
  EXPECT_TRUE(parsed.port.empty());
  EXPECT_TRUE(parsed.path.empty());
}

TEST(parse_url, http_without_explicit_port_defaults_to_80) {
  const http::parsed_url parsed = http::parse_url("http://example.com/path");
  EXPECT_EQ(parsed.protocol, "http");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.port, "80");
  EXPECT_EQ(parsed.path, "/path");
}

TEST(parse_url, https_without_explicit_port_defaults_to_443) {
  const http::parsed_url parsed = http::parse_url("https://example.com");
  EXPECT_EQ(parsed.protocol, "https");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.port, "443");
  EXPECT_EQ(parsed.path, "/");
}

TEST(parse_url, explicit_port_is_preserved) {
  const http::parsed_url parsed = http::parse_url("http://127.0.0.1:8080/api");
  EXPECT_EQ(parsed.host, "127.0.0.1");
  EXPECT_EQ(parsed.port, "8080");
  EXPECT_EQ(parsed.path, "/api");
}

TEST(parse_url, unknown_scheme_defaults_to_80_when_port_missing) {
  const http::parsed_url parsed = http::parse_url("ftp://example.com/resource");
  EXPECT_EQ(parsed.protocol, "ftp");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.port, "80");
  EXPECT_EQ(parsed.path, "/resource");
}

TEST(parse_url, scheme_only_yields_empty_host_and_default_http_port) {
  const http::parsed_url parsed = http::parse_url("http://");
  EXPECT_EQ(parsed.protocol, "http");
  EXPECT_TRUE(parsed.host.empty());
  EXPECT_EQ(parsed.port, "80");
  EXPECT_EQ(parsed.path, "/");
}

TEST(parse_url, empty_port_after_colon_is_preserved) {
  const http::parsed_url parsed = http::parse_url("http://example.com:/path");
  EXPECT_EQ(parsed.protocol, "http");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_TRUE(parsed.port.empty());
  EXPECT_EQ(parsed.path, "/path");
}

TEST(parse_url, query_and_fragment_are_kept_in_path) {
  const http::parsed_url parsed = http::parse_url("http://example.com/api?q=1#frag");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.path, "/api?q=1#frag");
}

TEST(parse_url, uppercase_protocol_is_preserved_and_defaults_to_80) {
  const http::parsed_url parsed = http::parse_url("HTTPS://example.com/api");
  EXPECT_EQ(parsed.protocol, "HTTPS");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.port, "80");
  EXPECT_EQ(parsed.path, "/api");
}

TEST(parse_url, userinfo_is_treated_as_host_prefix_with_colon_split) {
  const http::parsed_url parsed = http::parse_url("http://user:pass@example.com/secret");
  EXPECT_EQ(parsed.host, "user");
  EXPECT_EQ(parsed.port, "pass@example.com");
  EXPECT_EQ(parsed.path, "/secret");
}

TEST(parse_url, ipv6_bracket_literal_is_split_at_first_colon) {
  const http::parsed_url parsed = http::parse_url("http://[2001:db8::1]:443/path");
  EXPECT_EQ(parsed.host, "[2001");
  EXPECT_EQ(parsed.port, "db8::1]:443");
  EXPECT_EQ(parsed.path, "/path");
}

TEST(parse_url, path_traversal_segments_are_preserved_verbatim) {
  const http::parsed_url parsed = http::parse_url("http://example.com/../../windows/system32");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.path, "/../../windows/system32");
}

TEST(parse_url, encoded_separator_sequences_are_not_decoded) {
  const http::parsed_url parsed = http::parse_url("http://example.com/%2e%2e/%2fadmin%3a80");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.port, "80");
  EXPECT_EQ(parsed.path, "/%2e%2e/%2fadmin%3a80");
}

TEST(parse_url, host_header_injection_payload_is_retained_in_path_component) {
  const http::parsed_url parsed = http::parse_url("http://example.com/path\r\nInjected: yes");
  EXPECT_EQ(parsed.host, "example.com");
  EXPECT_EQ(parsed.path, "/path\r\nInjected: yes");
}

TEST(parse_url, request_smuggling_style_authority_is_not_interpreted_as_headers) {
  const http::parsed_url parsed = http::parse_url("http://example.com\r\nHost:evil.test/ok");
  EXPECT_EQ(parsed.host, "example.com\r\nHost");
  EXPECT_EQ(parsed.port, "evil.test");
  EXPECT_EQ(parsed.path, "/ok");
}

TEST(parse_url, embedded_nul_in_path_is_preserved_in_string_storage) {
  const std::string url("http://example.com/a\0b", 22);
  const http::parsed_url parsed = http::parse_url(url);
  EXPECT_EQ(parsed.host, "example.com");
  ASSERT_EQ(parsed.path.size(), 4u);
  EXPECT_EQ(parsed.path[0], '/');
  EXPECT_EQ(parsed.path[1], 'a');
  EXPECT_EQ(parsed.path[2], '\0');
  EXPECT_EQ(parsed.path[3], 'b');
}

// =============================================================================
// simple_client integration tests (loopback server)
// =============================================================================

TEST(simple_client, execute_reads_successful_response_and_body) {
  loopback_http_server server("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nhello");
  const http::http_client_options opts("http", "", "", "");
  http::simple_client client(opts);
  http::packet request("GET", "127.0.0.1", "/health");
  std::ostringstream os;

  const http::response resp = client.execute(os, "127.0.0.1", std::to_string(server.port()), request);

  EXPECT_EQ(resp.status_code_, 200u);
  EXPECT_NE(server.captured_request().find("GET /health HTTP/1.0"), std::string::npos);
  EXPECT_EQ(os.str(), "hello");
}

TEST(simple_client, execute_throws_for_non_2xx_response) {
  loopback_http_server server("HTTP/1.1 500 Internal Error\r\nContent-Type: text/plain\r\n\r\nfail");
  const http::http_client_options opts("http", "", "", "");
  http::simple_client client(opts);
  http::packet request("GET", "127.0.0.1", "/boom");
  std::ostringstream os;

  EXPECT_THROW(client.execute(os, "127.0.0.1", std::to_string(server.port()), request), socket_helpers::socket_exception);
}

TEST(simple_client, fetch_returns_payload_for_non_2xx_response) {
  loopback_http_server server("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nnot-found");
  const http::http_client_options opts("http", "", "", "");
  http::simple_client client(opts);
  http::packet request("GET", "127.0.0.1", "/missing");

  const http::response resp = client.fetch("127.0.0.1", std::to_string(server.port()), request);

  EXPECT_EQ(resp.status_code_, 404u);
  EXPECT_EQ(resp.payload_, "not-found");
}

TEST(simple_client, execute_throws_on_invalid_http_version) {
  loopback_http_server server("INVALID 200 OK\r\nHeader: value\r\n\r\nbody");
  const http::http_client_options opts("http", "", "", "");
  http::simple_client client(opts);
  http::packet request("GET", "127.0.0.1", "/invalid");
  std::ostringstream os;

  EXPECT_THROW(client.execute(os, "127.0.0.1", std::to_string(server.port()), request), socket_helpers::socket_exception);
}

TEST(simple_client, download_success_for_loopback_http_server) {
  loopback_http_server server("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\ncontent");
  std::ostringstream os;
  std::string error_msg;

  const bool ok = http::simple_client::download("http", "127.0.0.1", std::to_string(server.port()), "/file", "", "", "", os, error_msg);

  EXPECT_TRUE(ok);
  EXPECT_TRUE(error_msg.empty());
  EXPECT_EQ(os.str(), "content");
}

// =============================================================================
// http_packet parsing edge cases
// =============================================================================

TEST(http_packet, construct_from_raw_data_without_status_line_terminator_is_empty) {
  const std::string raw = "HTTP/1.1 200";
  const std::vector<char> data(raw.begin(), raw.end());
  const http::packet p(data);
  EXPECT_EQ(p.status_code_, 0);
  EXPECT_TRUE(p.payload_.empty());
}

TEST(http_packet, parse_http_response_invalid_status_code_throws) {
  http::packet p;
  EXPECT_ANY_THROW(p.parse_http_response("HTTP/1.1 ABC"));
}

TEST(http_packet_helpers, find_header_break_matches_lf_cr) {
  EXPECT_TRUE(http::find_header_break('\n', '\r'));
  EXPECT_FALSE(http::find_header_break('\r', '\n'));
}
