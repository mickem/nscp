// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the NRDP client's connection settings and what it posts.
//
// nrdp.cpp (the XML rendering) already has a suite; this covers the half that
// did not: connection_data, which turns a target's settings into a URL, and
// submit(), which decides whether a result is a host check or a service check
// and posts the form NRDP expects. A mistake in either is invisible until a
// monitoring server quietly stops seeing results.
//
// The POST is captured from a loopback HTTP server rather than asserted
// against an internal builder, because the form fields are the contract.

#include <client/command_line_parser.hpp>

#include "nrdp_client.hpp"

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <cstdlib>
#include <future>
#include <map>
#include <string>
#include <thread>

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

using boost::asio::ip::tcp;

client::destination_container target_with(const std::map<std::string, std::string> &options) {
  client::destination_container d;
  for (const auto &o : options) d.set_string_data(o.first, o.second);
  return d;
}

// Accepts one connection, keeps the request, answers with a canned NRDP
// response. Single-accept because a submit makes exactly one request, and a
// server waiting for a second would hang the suite on teardown.
class loopback_nrdp_server {
 public:
  loopback_nrdp_server() {
    std::promise<unsigned short> p;
    std::future<unsigned short> f = p.get_future();
    thread_ = std::thread([this, prom = std::move(p)]() mutable {
      try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, {tcp::v4(), 0});
        prom.set_value(acceptor.local_endpoint().port());
        tcp::socket socket(io);
        acceptor.accept(socket);

        boost::asio::streambuf buffer;
        boost::system::error_code ec;
        boost::asio::read_until(socket, buffer, "\r\n\r\n", ec);
        const std::string received((std::istreambuf_iterator<char>(&buffer)), std::istreambuf_iterator<char>());

        const std::string::size_type split = received.find("\r\n\r\n");
        headers_ = split == std::string::npos ? received : received.substr(0, split);
        std::string body = split == std::string::npos ? std::string() : received.substr(split + 4);

        // The rest of the body, if the headers arrived without it.
        std::size_t expected = 0;
        const std::string::size_type cl = headers_.find("Content-Length:");
        if (cl != std::string::npos) expected = std::strtoul(headers_.c_str() + cl + 15, nullptr, 10);
        while (body.size() < expected && !ec) {
          char chunk[4096];
          const std::size_t n = socket.read_some(boost::asio::buffer(chunk), ec);
          if (!ec) body.append(chunk, n);
        }
        body_ = body;

        const std::string payload = "<result><status>0</status><message>OK</message></result>";
        const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload;
        boost::asio::write(socket, boost::asio::buffer(response), ec);
      } catch (...) {
      }
    });
    port_ = f.get();
  }

  ~loopback_nrdp_server() {
    if (thread_.joinable()) thread_.join();
  }

  unsigned short port() const { return port_; }

  // Both join the server thread, so call them after the submit returns.
  std::string body() {
    if (thread_.joinable()) thread_.join();
    return body_;
  }
  std::string headers() {
    if (thread_.joinable()) thread_.join();
    return headers_;
  }

 private:
  unsigned short port_ = 0;
  std::string headers_;
  std::string body_;
  std::thread thread_;
};

// Submit one result to the loopback server and return the POST body.
std::string submit_one(loopback_nrdp_server &server, const std::string &alias, const PB::Common::ResultCode result, const std::string &message,
                       const std::string &token = "s3cret") {
  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  if (!alias.empty()) payload->set_alias(alias);
  payload->set_result(result);
  payload->add_lines()->set_message(message);

  client::destination_container sender;
  sender.set_string_data("host", "monitored-host");

  PB::Commands::SubmitResponseMessage response;
  nrdp_client::nrdp_client_handler handler;
  handler.submit(sender, target_with({{"address", "http://127.0.0.1:" + std::to_string(server.port()) + "/nrdp/server/"}, {"token", token}}), request, response);
  return server.body();
}

}  // namespace

// ---------------------------------------------------------------------------
// connection_data - a target's settings become a URL.
// ---------------------------------------------------------------------------

TEST(NrdpConnectionData, DefaultsToHttpOnPortEightyAtTheNrdpPath) {
  const nrdp_client::connection_data con(target_with({{"address", "nagios.example.com"}}), client::destination_container());

  EXPECT_EQ(con.protocol, "http");
  EXPECT_EQ(con.get_port(), "80");
  EXPECT_EQ(con.path, "/nrdp/server/") << "the default NRDP endpoint path";
}

TEST(NrdpConnectionData, HttpsDefaultsToPort443) {
  const nrdp_client::connection_data con(target_with({{"address", "https://nagios.example.com"}}), client::destination_container());

  EXPECT_EQ(con.protocol, "https");
  EXPECT_EQ(con.get_port(), "443");
}

TEST(NrdpConnectionData, AnExplicitPortAndPathWin) {
  const nrdp_client::connection_data con(target_with({{"address", "https://nagios.example.com:8443/custom/nrdp/"}}), client::destination_container());

  EXPECT_EQ(con.get_port(), "8443");
  EXPECT_EQ(con.path, "/custom/nrdp/");
}

TEST(NrdpConnectionData, AnUnknownProtocolIsTreatedAsPlainHttp) {
  // Anything that is not https falls back to http rather than being passed to
  // the http client as a scheme it cannot use.
  const nrdp_client::connection_data con(target_with({{"address", "gopher://nagios.example.com"}}), client::destination_container());

  EXPECT_EQ(con.protocol, "http");
  EXPECT_EQ(con.get_port(), "80");
}

TEST(NrdpConnectionData, CarriesTheTokenAndTlsSettings) {
  const nrdp_client::connection_data con(
      target_with({{"address", "https://h"}, {"token", "s3cret"}, {"tls version", "1.3"}, {"verify mode", "none"}, {"ca", "/etc/ca.pem"}}),
      client::destination_container());

  EXPECT_EQ(con.token, "s3cret");
  EXPECT_EQ(con.tls_version, "1.3");
  EXPECT_EQ(con.verify_mode, "none");
  EXPECT_EQ(con.ca, "/etc/ca.pem");
}

TEST(NrdpConnectionData, HttpsDefaultsVerifyModeToPeer) {
  // A missing verify mode must not silently disable certificate verification
  // on HTTPS: the TLS layer maps an empty string to verify_none, so the client
  // would submit the token to an unauthenticated (possibly MITM) server.
  const nrdp_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_EQ(con.verify_mode, "peer");
}

TEST(NrdpConnectionData, AnExplicitVerifyModeIsNotOverridden) {
  const nrdp_client::connection_data con(target_with({{"address", "https://h"}, {"verify mode", "none"}}), client::destination_container());

  EXPECT_EQ(con.verify_mode, "none") << "an operator opting out of verification must still be honoured";
}

TEST(NrdpConnectionData, PlainHttpLeavesVerifyModeEmpty) {
  // verify mode is meaningless without TLS, so http must not gain a spurious
  // default.
  const nrdp_client::connection_data con(target_with({{"address", "http://h"}}), client::destination_container());

  EXPECT_EQ(con.verify_mode, "");
}

TEST(NrdpConnectionData, ToStringDoesNotLeakTheToken) {
  // The token is a shared secret; to_string() is emitted at trace level on
  // every submission and must never carry it in the clear.
  const nrdp_client::connection_data con(target_with({{"address", "https://h"}, {"token", "s3cret-token"}}), client::destination_container());

  const std::string s = con.to_string();
  EXPECT_EQ(s.find("s3cret-token"), std::string::npos) << s;
  EXPECT_NE(s.find("<set>"), std::string::npos) << s;
}

TEST(NrdpConnectionData, ToStringMarksAnUnsetTokenWithoutLeaking) {
  const nrdp_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_NE(con.to_string().find("token: <unset>"), std::string::npos) << con.to_string();
}

TEST(NrdpConnectionData, ToStringRedactsProxyCredentials) {
  const nrdp_client::connection_data con(target_with({{"address", "https://h"}, {"proxy", "http://user:p4ss@proxy:3128/"}}),
                                         client::destination_container());

  const std::string s = con.to_string();
  EXPECT_EQ(s.find("p4ss"), std::string::npos) << s;
  EXPECT_EQ(s.find("user:"), std::string::npos) << s;
  EXPECT_NE(s.find("<redacted>@proxy:3128"), std::string::npos) << s;
}

TEST(NrdpRedactProxyUrl, LeavesACredentiallessUrlUnchanged) {
  EXPECT_EQ(nrdp_client::redact_proxy_url("http://proxy:3128/"), "http://proxy:3128/");
}

TEST(NrdpRedactProxyUrl, RedactsUserinfo) {
  EXPECT_EQ(nrdp_client::redact_proxy_url("http://user:pass@proxy:3128/"), "http://<redacted>@proxy:3128/");
}

TEST(NrdpRedactProxyUrl, DoesNotMistakeAPathAtForUserinfo) {
  // An '@' after the host (in the path) must not be treated as a userinfo
  // separator.
  EXPECT_EQ(nrdp_client::redact_proxy_url("http://proxy:3128/path@thing"), "http://proxy:3128/path@thing");
}

TEST(NrdpConnectionData, TlsVersionDefaultsToTwelveOrLater) {
  // Not a cosmetic default: it is what keeps the client off TLS 1.0/1.1 when
  // the target says nothing.
  const nrdp_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_EQ(con.tls_version, "1.2+");
}

TEST(NrdpConnectionData, TakesTheSenderHostnameFromTheSender) {
  // Only used in the trace line, but an empty one there is worse than useless
  // when a submit has gone to the wrong place.
  client::destination_container sender;
  sender.set_string_data("host", "reporting-host");

  const nrdp_client::connection_data con(target_with({{"address", "h"}}), sender);

  EXPECT_EQ(con.sender_hostname, "reporting-host");
  EXPECT_NE(con.to_string().find("reporting-host"), std::string::npos) << con.to_string();
}

// ---------------------------------------------------------------------------
// build_proxy_config - the no-proxy list is operator-typed, so it is parsed
// leniently.
// ---------------------------------------------------------------------------

TEST(NrdpProxyConfig, SplitsAndTrimsTheNoProxyList) {
  const nrdp_client::connection_data con(target_with({{"address", "h"}, {"proxy", "http://proxy:3128"}, {"no proxy", "localhost, .internal ,10.0.0.1"}}),
                                         client::destination_container());

  const http::proxy_config proxy = con.build_proxy_config();

  ASSERT_EQ(proxy.no_proxy.size(), 3u);
  EXPECT_EQ(proxy.no_proxy[0], "localhost");
  EXPECT_EQ(proxy.no_proxy[1], ".internal") << "surrounding spaces are not part of the host";
  EXPECT_EQ(proxy.no_proxy[2], "10.0.0.1");
}

TEST(NrdpProxyConfig, AnEmptyNoProxyListIsNoEntries) {
  const nrdp_client::connection_data con(target_with({{"address", "h"}, {"proxy", "http://proxy:3128"}}), client::destination_container());

  EXPECT_TRUE(con.build_proxy_config().no_proxy.empty());
}

// ---------------------------------------------------------------------------
// submit - the form NRDP expects.
// ---------------------------------------------------------------------------

TEST(NrdpSubmit, PostsTheSubmitcheckCommandWithTheToken) {
  loopback_nrdp_server server;

  const std::string body = submit_one(server, "disk", PB::Common::ResultCode::OK, "all good");

  EXPECT_NE(body.find("cmd=submitcheck"), std::string::npos) << body;
  EXPECT_NE(body.find("token=s3cret"), std::string::npos) << body;
  EXPECT_NE(body.find("XMLDATA="), std::string::npos) << body;
}

TEST(NrdpSubmit, PostsToTheConfiguredPath) {
  loopback_nrdp_server server;

  submit_one(server, "disk", PB::Common::ResultCode::OK, "all good");

  EXPECT_NE(server.headers().find("POST /nrdp/server/"), std::string::npos) << server.headers();
}

TEST(NrdpSubmit, ARegularResultIsReportedAsAServiceCheck) {
  loopback_nrdp_server server;

  const std::string body = submit_one(server, "disk", PB::Common::ResultCode::WARNING, "running low");

  // The XML is form-encoded, so look for the encoded markers.
  EXPECT_NE(body.find("servicename"), std::string::npos) << body;
  EXPECT_NE(body.find("disk"), std::string::npos) << body;
}

TEST(NrdpSubmit, TheAliasHostCheckIsReportedAsAHostCheck) {
  // "host_check" is the documented alias that turns a result into a host
  // check; NRDP tells the two apart by whether servicename is present.
  loopback_nrdp_server server;

  const std::string body = submit_one(server, "host_check", PB::Common::ResultCode::CRITICAL, "host is down");

  EXPECT_EQ(body.find("servicename"), std::string::npos) << body;
  EXPECT_NE(body.find("checkresult"), std::string::npos) << body;
}

TEST(NrdpSubmit, ACommandWithoutAnAliasIsNamedAfterTheCommand) {
  loopback_nrdp_server server;

  const std::string body = submit_one(server, "", PB::Common::ResultCode::OK, "fine");

  EXPECT_NE(body.find("check_something"), std::string::npos) << body;
}

TEST(NrdpConnectionData, CarriesTheConfiguredTimeoutAndRetry) {
  // Both are routed into typed fields by destination_container (like "host"),
  // so reading them out of the free-form data map silently returned the
  // default: a configured timeout/retry was ignored.
  client::destination_container target = target_with({{"address", "https://h"}, {"timeout", "7"}, {"retry", "5"}});

  const nrdp_client::connection_data con(target, client::destination_container());

  EXPECT_EQ(con.timeout, 7);
  EXPECT_EQ(con.retry, 5);
}

TEST(NrdpSubmit, AStalledServerTimesOutInsteadOfHangingForever) {
  // A server that accepts the connection and then never answers must not
  // wedge the submission thread: the configured timeout has to end the
  // exchange with an error. Before the timeout was wired through, this
  // submit blocked indefinitely (and this test would hang).
  std::promise<unsigned short> p;
  std::future<unsigned short> f = p.get_future();
  std::thread server([prom = std::move(p)]() mutable {
    try {
      boost::asio::io_context io;
      tcp::acceptor acceptor(io, {tcp::v4(), 0});
      prom.set_value(acceptor.local_endpoint().port());
      tcp::socket socket(io);
      acceptor.accept(socket);
      // Hold the socket open without answering until the client gives up.
      char buf[1024];
      boost::system::error_code ec;
      while (!ec) socket.read_some(boost::asio::buffer(buf), ec);
    } catch (...) {
    }
  });
  const unsigned short port = f.get();

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("fine");
  client::destination_container sender;
  sender.set_string_data("host", "monitored-host");

  PB::Commands::SubmitResponseMessage response;
  nrdp_client::nrdp_client_handler handler;
  handler.submit(sender, target_with({{"address", "http://127.0.0.1:" + std::to_string(port) + "/nrdp/"}, {"token", "t"}, {"timeout", "1"}, {"retry", "1"}}),
                 request, response);
  server.join();

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  EXPECT_NE(response.payload(0).result().message().find("timed out"), std::string::npos) << response.payload(0).result().message();
}

TEST(NrdpSubmit, AnHttpErrorStatusIsReportedNotParsed) {
  // A non-2xx answer is the server speaking, not a transport failure: it must
  // come back as an error naming the status, and must not be retried or fed
  // to the XML parser (whose "Invalid response" would hide what happened).
  std::promise<unsigned short> p;
  std::future<unsigned short> f = p.get_future();
  std::thread server([prom = std::move(p)]() mutable {
    try {
      boost::asio::io_context io;
      tcp::acceptor acceptor(io, {tcp::v4(), 0});
      prom.set_value(acceptor.local_endpoint().port());
      tcp::socket socket(io);
      acceptor.accept(socket);
      boost::asio::streambuf buffer;
      boost::system::error_code ec;
      boost::asio::read_until(socket, buffer, "\r\n\r\n", ec);
      const std::string body = "denied";
      const std::string response_text =
          "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
      boost::asio::write(socket, boost::asio::buffer(response_text), ec);
    } catch (...) {
    }
  });
  const unsigned short port = f.get();

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("fine");
  client::destination_container sender;
  sender.set_string_data("host", "monitored-host");

  PB::Commands::SubmitResponseMessage response;
  nrdp_client::nrdp_client_handler handler;
  handler.submit(sender, target_with({{"address", "http://127.0.0.1:" + std::to_string(port) + "/nrdp/"}, {"token", "t"}}), request, response);
  server.join();

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  EXPECT_NE(response.payload(0).result().message().find("403"), std::string::npos) << response.payload(0).result().message();
}

TEST(NrdpSubmit, ATransportFailureIsRetried) {
  // First connection is accepted and dropped without a byte; the second gets a
  // proper answer. With retry=2 the submission must succeed on the second
  // attempt rather than reporting the dropped connection.
  std::promise<unsigned short> p;
  std::future<unsigned short> f = p.get_future();
  std::thread server([prom = std::move(p)]() mutable {
    try {
      boost::asio::io_context io;
      tcp::acceptor acceptor(io, {tcp::v4(), 0});
      prom.set_value(acceptor.local_endpoint().port());
      {
        tcp::socket first(io);
        acceptor.accept(first);
        // Closed by scope exit: the client sees EOF before any response.
      }
      tcp::socket second(io);
      acceptor.accept(second);
      boost::asio::streambuf buffer;
      boost::system::error_code ec;
      boost::asio::read_until(second, buffer, "\r\n\r\n", ec);
      const std::string body = "<result><status>0</status><message>OK</message></result>";
      const std::string response_text = "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
      boost::asio::write(second, boost::asio::buffer(response_text), ec);
    } catch (...) {
    }
  });
  const unsigned short port = f.get();

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("fine");
  client::destination_container sender;
  sender.set_string_data("host", "monitored-host");

  PB::Commands::SubmitResponseMessage response;
  nrdp_client::nrdp_client_handler handler;
  handler.submit(sender, target_with({{"address", "http://127.0.0.1:" + std::to_string(port) + "/nrdp/"}, {"token", "t"}, {"retry", "2"}, {"timeout", "5"}}),
                 request, response);
  server.join();

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_OK) << response.payload(0).result().message();
}

TEST(NrdpSubmit, OnlySubmitIsSupported) {
  // NRDP is a one-way result feed: there is nothing to query or execute.
  nrdp_client::nrdp_client_handler handler;
  const client::destination_container empty;

  PB::Commands::QueryRequestMessage query_request;
  PB::Commands::QueryResponseMessage query_response;
  EXPECT_FALSE(handler.query(empty, empty, query_request, query_response));

  PB::Commands::ExecuteRequestMessage exec_request;
  PB::Commands::ExecuteResponseMessage exec_response;
  EXPECT_FALSE(handler.exec(empty, empty, exec_request, exec_response));

  PB::Metrics::MetricsMessage metrics;
  EXPECT_FALSE(handler.metrics(empty, empty, metrics));
}
