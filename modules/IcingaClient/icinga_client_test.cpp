// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the Icinga 2 client's connection settings and its submissions.
//
// icinga.cpp (the JSON body and the response parsing) already has a suite;
// this covers icinga_client.hpp, which had none: connection_data, which turns
// a target's settings into an API endpoint, and submit(), which posts a check
// result to /v1/actions/process-check-result and turns the answer back into a
// submit response.
//
// The request is captured from a loopback HTTP server, so what is asserted is
// the API call Icinga would receive - the path, the credentials and the body.

#include <client/command_line_parser.hpp>

#include "icinga_client.hpp"

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

// Accepts one connection, keeps the request, and answers with a caller-chosen
// status and body.
class loopback_icinga_api {
 public:
  explicit loopback_icinga_api(std::string response_body = R"({"results":[{"code":200,"status":"Successfully processed check result"}]})",
                               const int status = 200)
      : body_to_send_(std::move(response_body)), status_(status) {
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

        std::size_t expected = 0;
        const std::string::size_type cl = headers_.find("Content-Length:");
        if (cl != std::string::npos) expected = std::strtoul(headers_.c_str() + cl + 15, nullptr, 10);
        while (body.size() < expected && !ec) {
          char chunk[4096];
          const std::size_t n = socket.read_some(boost::asio::buffer(chunk), ec);
          if (!ec) body.append(chunk, n);
        }
        request_body_ = body;

        const std::string response = "HTTP/1.1 " + std::to_string(status_) + " x\r\nContent-Type: application/json\r\nContent-Length: " +
                                     std::to_string(body_to_send_.size()) + "\r\n\r\n" + body_to_send_;
        boost::asio::write(socket, boost::asio::buffer(response), ec);
      } catch (...) {
      }
    });
    port_ = f.get();
  }

  ~loopback_icinga_api() {
    if (thread_.joinable()) thread_.join();
  }

  unsigned short port() const { return port_; }
  std::string request_body() {
    if (thread_.joinable()) thread_.join();
    return request_body_;
  }
  std::string headers() {
    if (thread_.joinable()) thread_.join();
    return headers_;
  }

 private:
  std::string body_to_send_;
  int status_;
  unsigned short port_ = 0;
  std::string headers_;
  std::string request_body_;
  std::thread thread_;
};

// Submit one result, and hand back the response payload so the caller can
// check how the API's answer was interpreted.
PB::Commands::SubmitResponseMessage submit_one(loopback_icinga_api &api, const std::string &alias, const PB::Common::ResultCode result,
                                               const std::string &message, std::map<std::string, std::string> extra = {}) {
  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  if (!alias.empty()) payload->set_alias(alias);
  payload->set_result(result);
  payload->add_lines()->set_message(message);

  extra["address"] = "http://127.0.0.1:" + std::to_string(api.port());
  if (extra.find("username") == extra.end()) extra["username"] = "root";
  if (extra.find("password") == extra.end()) extra["password"] = "icinga";

  client::destination_container sender;
  sender.set_host("monitored-host");

  PB::Commands::SubmitResponseMessage response;
  icinga_client::icinga_client_handler handler;
  handler.submit(sender, target_with(extra), request, response);
  return response;
}

}  // namespace

// ---------------------------------------------------------------------------
// connection_data
// ---------------------------------------------------------------------------

TEST(IcingaConnectionData, DefaultsToHttpsOnTheApiPort) {
  const icinga_client::connection_data con(target_with({{"address", "icinga.example.com"}}), client::destination_container());

  EXPECT_EQ(con.protocol, "https") << "Icinga 2's API is https-only in practice";
  EXPECT_EQ(con.get_port(), "5665");
}

TEST(IcingaConnectionData, PlainHttpIsHonouredForATestSetup) {
  const icinga_client::connection_data con(target_with({{"address", "http://icinga.example.com"}}), client::destination_container());

  EXPECT_EQ(con.protocol, "http");
  EXPECT_EQ(con.get_port(), "5665");
}

TEST(IcingaConnectionData, AnExplicitPortWins) {
  const icinga_client::connection_data con(target_with({{"address", "https://icinga.example.com:8443"}}), client::destination_container());

  EXPECT_EQ(con.get_port(), "8443");
}

TEST(IcingaConnectionData, CarriesTheApiCredentials) {
  const icinga_client::connection_data con(target_with({{"address", "https://h"}, {"username", "root"}, {"password", "s3cret"}}),
                                           client::destination_container());

  EXPECT_EQ(con.username, "root");
  EXPECT_EQ(con.password, "s3cret");
}

TEST(IcingaConnectionData, TlsVersionDefaultsToOneThree) {
  const icinga_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_EQ(con.tls_version, "1.3");
}

TEST(IcingaConnectionData, ObjectTemplatesAndEnsureFlagAreRead) {
  const icinga_client::connection_data con(target_with({{"address", "https://h"},
                                                        {"ensure_objects", "true"},
                                                        {"host_template", "generic-host"},
                                                        {"service_template", "generic-service"},
                                                        {"check_command", "dummy"}}),
                                           client::destination_container());

  EXPECT_TRUE(con.ensure_objects);
  EXPECT_EQ(con.host_template, "generic-host");
  EXPECT_EQ(con.service_template, "generic-service");
  EXPECT_EQ(con.check_command, "dummy");
}

TEST(IcingaConnectionData, TheSenderNamesTheHostAndTheCheckSource) {
  client::destination_container sender;
  sender.set_host("monitored-host");

  const icinga_client::connection_data con(target_with({{"address", "https://h"}}), sender);

  EXPECT_EQ(con.sender_hostname, "monitored-host");
  EXPECT_EQ(con.check_source, "monitored-host") << "check_source defaults to whoever is reporting";
}

TEST(IcingaConnectionData, AnExplicitCheckSourceIsKept) {
  client::destination_container sender;
  sender.set_host("monitored-host");

  const icinga_client::connection_data con(target_with({{"address", "https://h"}, {"check_source", "nscp-agent"}}), sender);

  EXPECT_EQ(con.check_source, "nscp-agent");
}

TEST(IcingaConnectionData, WithoutASenderTheLocalHostNameIsUsed) {
  // submit_icinga from the CLI has no sender, and Icinga rejects a check
  // result with an empty host, so something has to fill it in.
  const icinga_client::connection_data con(target_with({{"address", "https://h"}}), client::destination_container());

  EXPECT_FALSE(con.sender_hostname.empty());
  EXPECT_EQ(con.sender_hostname, boost::asio::ip::host_name());
}

// ---------------------------------------------------------------------------
// submit - the API call Icinga receives.
// ---------------------------------------------------------------------------

TEST(IcingaSubmit, PostsAProcessCheckResultAction) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::WARNING, "running low");

  EXPECT_NE(api.headers().find("POST /v1/actions/process-check-result"), std::string::npos) << api.headers();
  // Icinga only answers json, and the API requires the client to say so.
  EXPECT_NE(api.headers().find("Accept: application/json"), std::string::npos) << api.headers();
}

TEST(IcingaSubmit, AuthenticatesWithTheConfiguredCredentials) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::OK, "fine");

  // root:icinga
  EXPECT_NE(api.headers().find("Authorization: Basic cm9vdDppY2luZ2E="), std::string::npos) << api.headers();
}

TEST(IcingaSubmit, AServiceResultFiltersOnHostAndService) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::WARNING, "running low");

  const std::string body = api.request_body();
  EXPECT_NE(body.find("\"type\":\"Service\""), std::string::npos) << body;
  EXPECT_NE(body.find("host.name==\\\"monitored-host\\\""), std::string::npos) << body;
  EXPECT_NE(body.find("service.name==\\\"disk\\\""), std::string::npos) << body;
  EXPECT_NE(body.find("\"exit_status\":1"), std::string::npos) << body;
}

TEST(IcingaSubmit, TheHostCheckAliasFiltersOnTheHostAlone) {
  loopback_icinga_api api;

  submit_one(api, "host_check", PB::Common::ResultCode::CRITICAL, "host is down");

  const std::string body = api.request_body();
  EXPECT_NE(body.find("\"type\":\"Host\""), std::string::npos) << body;
  EXPECT_EQ(body.find("service.name"), std::string::npos) << body;
  // A host is UP or DOWN: CRITICAL maps onto DOWN (1), not the service's 2.
  EXPECT_NE(body.find("\"exit_status\":1"), std::string::npos) << body;
}

TEST(IcingaSubmit, PerfdataIsSplitOutOfThePluginOutput) {
  loopback_icinga_api api;

  submit_one(api, "disk", PB::Common::ResultCode::OK, "all good|'used'=42%;80;90");

  const std::string body = api.request_body();
  EXPECT_NE(body.find("\"performance_data\""), std::string::npos) << body;
  EXPECT_NE(body.find("'used'=42%;80;90"), std::string::npos) << body;
  // The perfdata must not be left in the human-readable output as well.
  EXPECT_NE(body.find("\"plugin_output\":\"all good\""), std::string::npos) << body;
}

TEST(IcingaSubmit, ASuccessfulSubmissionIsReportedAsGood) {
  loopback_icinga_api api;

  const PB::Commands::SubmitResponseMessage response = submit_one(api, "disk", PB::Common::ResultCode::OK, "fine");

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_OK);
  EXPECT_NE(response.payload(0).result().message().find("Successfully processed"), std::string::npos) << response.payload(0).result().message();
}

TEST(IcingaSubmit, AnApiErrorIsReportedRatherThanSwallowed) {
  // A submission that Icinga rejected must not look like a delivery.
  loopback_icinga_api api(R"({"results":[{"code":404,"status":"No objects found."}]})", 404);

  const PB::Commands::SubmitResponseMessage response = submit_one(api, "disk", PB::Common::ResultCode::OK, "fine");

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
  EXPECT_NE(response.payload(0).result().message().find("No objects found"), std::string::npos) << response.payload(0).result().message();
}

TEST(IcingaSubmit, AnUnreachableApiIsReportedAsAnError) {
  // Nothing is listening on a port the kernel has just handed back.
  boost::asio::io_context io;
  tcp::acceptor probe(io, {tcp::v4(), 0});
  const unsigned short closed = probe.local_endpoint().port();
  probe.close();

  PB::Commands::SubmitRequestMessage request;
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_something");
  payload->set_result(PB::Common::ResultCode::OK);
  payload->add_lines()->set_message("fine");

  PB::Commands::SubmitResponseMessage response;
  icinga_client::icinga_client_handler handler;
  handler.submit(client::destination_container(), target_with({{"address", "http://127.0.0.1:" + std::to_string(closed)}}), request, response);

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result().code(), PB::Common::Result_StatusCodeType_STATUS_ERROR);
}

TEST(IcingaSubmit, QueryAndExecAreNotSupported) {
  icinga_client::icinga_client_handler handler;
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
