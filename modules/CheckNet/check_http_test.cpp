// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// End-to-end tests for the CheckNet checks that speak HTTP: check_http, the
// four status-page checks (check_apache_status, check_nginx_status,
// check_phpfpm_status, check_tomcat_status) and check_nsclient_web_online.
// Each runs the real check entry point against a scripted HTTP server on
// 127.0.0.1 and asserts on what the check sent as well as what it reported.

#include "check_http.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <chrono>
#include <string>
#include <vector>

#include "check_nsclient_web_online.h"
#include "check_webserver.h"
#include "loopback_test_support.hpp"

namespace {

using namespace check_net_test;

typedef void (*check_fn)(const std::string &, const PB::Commands::QueryRequestMessage::Request &, PB::Commands::QueryResponseMessage::Response *);

struct outcome {
  PB::Common::ResultCode code;
  std::string message;
  std::string perf;
};

outcome run(check_fn check, const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  check("none", request, &response);
  outcome r;
  r.code = response.result();
  for (int i = 0; i < response.lines_size(); ++i) {
    if (!r.message.empty()) r.message += "\n";
    r.message += response.lines(i).message();
    for (int j = 0; j < response.lines(i).perf_size(); ++j) r.perf += response.lines(i).perf(j).alias() + ";";
  }
  return r;
}

std::string base(const scripted_server &server) { return "http://127.0.0.1:" + server.port(); }

bool contains(const std::string &haystack, const std::string &needle) { return haystack.find(needle) != std::string::npos; }

const char kApacheAuto[] =
    "localhost\n"
    "ServerVersion: Apache/2.4.58 (Unix)\n"
    "ServerUptimeSeconds: 7254\n"
    "Total Accesses: 8341\n"
    "Total kBytes: 91674\n"
    "Uptime: 7254\n"
    "ReqPerSec: 1.14985\n"
    "BytesPerSec: 12940.7\n"
    "BusyWorkers: 3\n"
    "IdleWorkers: 47\n"
    "Scoreboard: __W_K_____W.....................\n";

const char kNginxStub[] =
    "Active connections: 291 \n"
    "server accepts handled requests\n"
    " 16630948 16630946 31070465 \n"
    "Reading: 6 Writing: 179 Waiting: 106 \n";

const char kPhpFpm[] =
    "pool:                 www\n"
    "process manager:      dynamic\n"
    "start time:           01/Jul/2024:10:00:04 +0200\n"
    "start since:          7254\n"
    "accepted conn:        4211\n"
    "listen queue:         2\n"
    "max listen queue:     11\n"
    "listen queue len:     511\n"
    "idle processes:       7\n"
    "active processes:     3\n"
    "total processes:      10\n"
    "max active processes: 9\n"
    "max children reached: 1\n"
    "slow requests:        5\n";

const char kTomcatXml[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<status><jvm><memory free='1734127416' total='2147483648' max='4294967296'/></jvm>"
    "<connector name='\"http-nio-8080\"'><threadInfo maxThreads=\"200\" currentThreadCount=\"25\" currentThreadsBusy=\"190\"/>"
    "<requestInfo maxTime=\"1230\" processingTime=\"55211\" requestCount=\"104211\" errorCount=\"17\" bytesReceived=\"0\" bytesSent=\"1048576000\"/>"
    "<workers></workers></connector>"
    "<connector name='\"ajp-nio-8009\"'><threadInfo maxThreads=\"100\" currentThreadCount=\"0\" currentThreadsBusy=\"0\"/>"
    "<requestInfo maxTime=\"0\" processingTime=\"0\" requestCount=\"0\" errorCount=\"0\" bytesReceived=\"0\" bytesSent=\"0\"/>"
    "<workers></workers></connector></status>";

}  // namespace

// ============================================================================
// check_http: arguments
// ============================================================================

TEST(CheckHttpArgs, NeedsAUrlOrAHost) {
  const outcome r = run(&check_net::check_http, {"path=/x"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("No URL or host specified", r.message);
}

TEST(CheckHttpArgs, RejectsAnUnknownProtocol) {
  const outcome r = run(&check_net::check_http, {"host=127.0.0.1", "protocol=gopher"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("Invalid protocol: gopher", r.message);
}

TEST(CheckHttpArgs, RejectsAJsonPathWithoutAlias) {
  const outcome r = run(&check_net::check_http, {"url=http://127.0.0.1/", "json-path=data.value"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("Invalid --json-path (expected alias:path): data.value", r.message);
}

TEST(CheckHttpArgs, RejectsAnUnknownAddressFamily) {
  const outcome r = run(&check_net::check_http, {"url=http://127.0.0.1/", "address-family=ipx"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("Invalid address-family: ipx (expected any, ipv4 or ipv6)", r.message);
}

// ============================================================================
// check_http: requests and results
// ============================================================================

TEST(CheckHttp, GetIsOkAndSendsTheExpectedRequest) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, "hello world"), &seen));
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/status?x=1", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "/status?x=1 -> 200 ok (11B in")) << r.message;
  EXPECT_EQ("GET /status?x=1 HTTP/1.0", seen.request_line());
  EXPECT_EQ("NSClient++", seen.header("User-Agent"));
  EXPECT_TRUE(contains(seen.header("Host"), "127.0.0.1")) << seen.head;
  // time and code are threshold perfdata; size is added explicitly.
  EXPECT_TRUE(contains(r.perf, "_size")) << r.perf;
}

TEST(CheckHttp, HostPortAndPathBuildTheUrl) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, "x"), &seen));
  const outcome r = run(&check_net::check_http, {"host=127.0.0.1", "port=" + server.port(), "path=health", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "http://127.0.0.1:" + server.port() + "/health -> 200")) << r.message;
  EXPECT_EQ("GET /health HTTP/1.0", seen.request_line());
}

TEST(CheckHttp, ErrorStatusIsCritical) {
  scripted_server server(http_reply(http_response(503, "down")));
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "-> 503 http_503")) << r.message;
}

TEST(CheckHttp, MissingExpectedBodyIsNoMatch) {
  scripted_server server(http_reply(http_response(200, "maintenance mode")));
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/", "expected-body=all systems go"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "200 no_match")) << r.message;
}

TEST(CheckHttp, PostDataHeadersAndBasicAuthAreSent) {
  http_request seen;
  scripted_server server(http_reply(http_response(201, "created"), &seen));
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/api", "post-data=a=1&b=2", "username=admin", "password=s3cret",
                                                 "header=X-Trace: abc", "header=not a header", "user-agent=probe/1.0", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  // A body without an explicit method is a POST.
  EXPECT_EQ("POST /api HTTP/1.0", seen.request_line());
  EXPECT_EQ("a=1&b=2", seen.body);
  EXPECT_EQ("7", seen.header("Content-Length"));
  EXPECT_EQ("application/x-www-form-urlencoded", seen.header("Content-Type"));
  EXPECT_EQ("Basic YWRtaW46czNjcmV0", seen.header("Authorization"));
  EXPECT_EQ("abc", seen.header("X-Trace"));
  EXPECT_EQ("probe/1.0", seen.header("User-Agent"));
}

TEST(CheckHttp, ExplicitMethodIsUppercased) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, ""), &seen));
  run(&check_net::check_http, {"url=" + base(server) + "/", "method=delete"});
  EXPECT_EQ("DELETE / HTTP/1.0", seen.request_line());
}

TEST(CheckHttp, RedirectIsReportedAsIsByDefault) {
  scripted_server server(http_reply(http_response(302, "", "Location: /elsewhere\r\n")));
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "-> 302 ok")) << r.message;
}

TEST(CheckHttp, RedirectIsFollowedWhenAsked) {
  http_request first, second;
  scripted_server server(
      std::vector<script_type>{http_reply(http_response(301, "", "Location: /new-home\r\n"), &first), http_reply(http_response(200, "you made it"), &second)});
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/old", "onredirect=follow", "expected-body=made it", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_EQ("GET /old HTTP/1.0", first.request_line());
  EXPECT_EQ("GET /new-home HTTP/1.0", second.request_line());
}

TEST(CheckHttp, RedirectLimitStopsFollowing) {
  scripted_server server(
      std::vector<script_type>{http_reply(http_response(302, "", "Location: /a\r\n")), http_reply(http_response(302, "", "Location: /b\r\n"))});
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/", "onredirect=follow", "max-redirs=1", "show-all"});
  // The second redirect is past the limit, so it is the answer (and < 400 is ok).
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "-> 302 ok")) << r.message;
}

TEST(CheckHttp, JsonPathBecomesAKeyword) {
  scripted_server server(http_reply(http_response(200, R"({"data":{"queue":{"length":250,"name":"jobs"}}})")));
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/q", "json-path=qlen:data.queue.length", "json-path=qname:data.queue.name",
                                                 "critical=qlen > 100", "detail-syntax=${qname}=${qlen}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "jobs=250")) << r.message;
  EXPECT_TRUE(contains(r.perf, "qlen")) << r.perf;
}

TEST(CheckHttp, InvalidUrlIsReported) {
  const outcome r = run(&check_net::check_http, {"url=not a url", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "invalid_url")) << r.message;
}

TEST(CheckHttp, ConnectionFailureIsAnError) {
  const outcome r = run(&check_net::check_http, {"url=http://127.0.0.1:" + closed_port() + "/", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "error: ")) << r.message;
}

TEST(CheckHttp, SilentServerTimesOut) {
  // A server that accepts and never answers must not hold the check: timeout=
  // bounds it (applied in whole seconds, so 1000 ms is one second).
  scripted_server server([](tcp::socket &s) { drain(s); });
  const auto start = std::chrono::steady_clock::now();
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/", "timeout=1000", "detail-syntax=${result}"});
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "error: ")) << r.message;
  EXPECT_LT(elapsed, std::chrono::seconds(10));
}

TEST(CheckHttpArgs, RejectsANonPositiveTimeout) {
  const outcome r = run(&check_net::check_http, {"url=http://127.0.0.1/", "timeout=0"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("timeout must be a positive number of milliseconds", r.message);
}

TEST(CheckHttp, EveryUrlIsChecked) {
  scripted_server server(std::vector<script_type>{http_reply(http_response(200, "a")), http_reply(http_response(404, "b"))});
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/one", "url=" + base(server) + "/two", "detail-syntax=${path}:${code}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "/two:404")) << r.message;
}

TEST(CheckHttp, PlainHttpDoesNotCoverRequiredSans) {
  scripted_server server(http_reply(http_response(200, "ok")));
  const outcome r = run(&check_net::check_http, {"url=" + base(server) + "/", "sans=www.example.com", "detail-syntax=${result} ${missing_sans}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "san_missing www.example.com")) << r.message;
}

#ifdef USE_SSL

TEST(CheckHttps, ServesOverTlsAndReportsTheCertificate) {
  test_identity id;
  http_request seen;
  scripted_server server(tls_server(id, [&seen](boost::asio::ssl::stream<tcp::socket &> &stream) {
    std::string data;
    char buf[4096];
    boost::system::error_code ec;
    while (!ec && data.find("\r\n\r\n") == std::string::npos) data.append(buf, stream.read_some(boost::asio::buffer(buf), ec));
    seen.head = data;
    const std::string reply = http_response(200, "secure");
    boost::asio::write(stream, boost::asio::buffer(reply), ec);
    stream.shutdown(ec);
  }));
  const outcome r = run(&check_net::check_http, {"url=https://127.0.0.1:" + server.port() + "/", "verify=none", "sans=localhost",
                                                 "detail-syntax=${result} ${code} ${cert_cn} ${cert_self_signed}", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "ok 200 localhost 1")) << r.message;
  EXPECT_TRUE(contains(seen.head, "GET / HTTP/1.0")) << seen.head;
}

TEST(CheckHttps, UntrustedCertificateFailsUnderVerifyPeer) {
  test_identity id;
  scripted_server server(tls_server(id));
  const outcome r = run(&check_net::check_http, {"url=https://127.0.0.1:" + server.port() + "/", "sni=localhost", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "error: ")) << r.message;
}

#endif  // USE_SSL

// ============================================================================
// Status pages
// ============================================================================

TEST(CheckApacheStatus, ParsesModStatusAndAppendsAuto) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, kApacheAuto), &seen));
  const outcome r = run(&check_net::check_apache_status, {"url=" + base(server) + "/server-status", "username=ops", "password=pw"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "ok: 3 busy and 47 idle workers")) << r.message;
  EXPECT_TRUE(contains(r.message, "uptime 7254s")) << r.message;
  // mod_status only answers machine-readable with ?auto.
  EXPECT_EQ("GET /server-status?auto HTTP/1.0", seen.request_line());
  EXPECT_EQ("Basic b3BzOnB3", seen.header("Authorization"));
  EXPECT_TRUE(contains(r.perf, "_busy_workers")) << r.perf;
}

TEST(CheckApacheStatus, HtmlPageIsAParseError) {
  scripted_server server(http_reply(http_response(200, "<html><body>Apache Status</body></html>")));
  const outcome r = run(&check_net::check_apache_status, {"url=" + base(server) + "/server-status", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "parse_error")) << r.message;
}

TEST(CheckApacheStatus, NonPositiveTimeoutIsRejected) {
  const outcome r = run(&check_net::check_apache_status, {"url=http://127.0.0.1/server-status", "timeout=0"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "timeout must be a positive number of seconds")) << r.message;
}

TEST(CheckNginxStatus, ParsesStubStatus) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, kNginxStub), &seen));
  const outcome r = run(&check_net::check_nginx_status, {"url=" + base(server) + "/nginx_status", "detail-syntax=${active} ${requests} ${dropped}"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "291 31070465 2")) << r.message;
  EXPECT_EQ("GET /nginx_status HTTP/1.0", seen.request_line());
}

TEST(CheckNginxStatus, ServerErrorIsCritical) {
  scripted_server server(http_reply(http_response(500, "oops")));
  const outcome r = run(&check_net::check_nginx_status, {"url=" + base(server) + "/nginx_status", "detail-syntax=${result} ${code}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "http_500 500")) << r.message;
}

TEST(CheckNginxStatus, InvalidUrlIsReported) {
  const outcome r = run(&check_net::check_nginx_status, {"url=nonsense", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "invalid_url")) << r.message;
}

TEST(CheckNginxStatus, UnreachableServerIsAnError) {
  const outcome r = run(&check_net::check_nginx_status, {"url=http://127.0.0.1:" + closed_port() + "/nginx_status", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "error: ")) << r.message;
}

TEST(CheckPhpFpmStatus, QueuedRequestsWarnByDefault) {
  scripted_server server(http_reply(http_response(200, kPhpFpm)));
  const outcome r = run(&check_net::check_phpfpm_status, {"url=" + base(server) + "/status"});
  // listen_queue > 0 is the default warning.
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "ok: pool www: 3 active, 7 idle, 2 queued")) << r.message;
}

TEST(CheckPhpFpmStatus, NotFoundPageIsAParseError) {
  scripted_server server(http_reply(http_response(200, "File not found.\n")));
  const outcome r = run(&check_net::check_phpfpm_status, {"url=" + base(server) + "/status", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "parse_error")) << r.message;
}

TEST(CheckTomcatStatus, OneRecordPerConnector) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, kTomcatXml), &seen));
  const outcome r = run(&check_net::check_tomcat_status, {"url=" + base(server) + "/manager/status", "show-all"});
  // http-nio-8080 has 190 of 200 threads busy: 95% is over the 90% critical.
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "http-nio-8080 ok: 190/200 threads busy")) << r.message;
  EXPECT_TRUE(contains(r.message, "ajp-nio-8009 ok: 0/100 threads busy")) << r.message;
  EXPECT_EQ("GET /manager/status?XML=true HTTP/1.0", seen.request_line());
}

TEST(CheckTomcatStatus, NoConnectorsIsAParseError) {
  scripted_server server(http_reply(http_response(200, "<status><jvm><memory free='1' total='2' max='3'/></jvm></status>")));
  const outcome r = run(&check_net::check_tomcat_status, {"url=" + base(server) + "/manager/status", "detail-syntax=${result} ${memory_max}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "parse_error 3")) << r.message;
}

TEST(CheckTomcatStatus, AuthenticationFailureIsReported) {
  scripted_server server(http_reply(http_response(401, "Unauthorized")));
  const outcome r = run(&check_net::check_tomcat_status, {"url=" + base(server) + "/manager/status", "detail-syntax=${result}"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "http_401")) << r.message;
}

// ============================================================================
// check_nsclient_web_online
// ============================================================================

TEST(CheckNsclientWebOnlineRun, NeedsAHost) {
  const outcome r = run(&check_net::check_nsclient_web_online, {"port=8443"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code);
  EXPECT_EQ("No host or url specified", r.message);
}

TEST(CheckNsclientWebOnlineRun, InfoEndpointMeansReachable) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, "{}"), &seen));
  const outcome r = run(&check_net::check_nsclient_web_online, {"url=" + base(server), "password=topsecret"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_EQ("REST API reachable on " + base(server), r.message);
  EXPECT_EQ("GET /api/v1/info HTTP/1.0", seen.request_line());
  // A password without a user goes in the legacy password header.
  EXPECT_EQ("topsecret", seen.header("password"));
  EXPECT_EQ("", seen.header("Authorization"));
}

TEST(CheckNsclientWebOnlineRun, RemoteCommandResultIsRelayed) {
  http_request seen;
  scripted_server server(http_reply(http_response(200, R"({"command":"check_cpu","result":1,"lines":[{"message":"CPU load is 85%"}]})"), &seen));
  // CLI-style tokens: "--key" then "value", repeated arguments.
  const outcome r = run(&check_net::check_nsclient_web_online, {"--url", base(server) + "/ignored/path", "--user", "admin", "--password", "pw", "--command",
                                                                "check_cpu", "--argument", "warn=load > 80", "--argument", "show-all"});
  EXPECT_EQ(PB::Common::ResultCode::WARNING, r.code) << r.message;
  EXPECT_EQ("CPU load is 85%", r.message);
  EXPECT_EQ("GET /api/v1/queries/check_cpu/commands/execute?warn=load%20%3E%2080&show-all HTTP/1.0", seen.request_line());
  EXPECT_EQ("Basic YWRtaW46cHc=", seen.header("Authorization"));
}

TEST(CheckNsclientWebOnlineRun, EmptyRemoteMessageIsExplained) {
  scripted_server server(http_reply(http_response(200, R"({"result":0,"lines":[]})")));
  const outcome r = run(&check_net::check_nsclient_web_online, {"url=" + base(server), "command=check_ok"});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.message;
  EXPECT_EQ("(no message returned by check_ok)", r.message);
}

TEST(CheckNsclientWebOnlineRun, UnknownRemoteCodeIsUnknown) {
  scripted_server server(http_reply(http_response(200, R"({"result":7,"lines":[{"message":"odd"}]})")));
  const outcome r = run(&check_net::check_nsclient_web_online, {"url=" + base(server), "command=check_odd"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
}

TEST(CheckNsclientWebOnlineRun, UnparsableBodyIsUnknown) {
  scripted_server server(http_reply(http_response(200, "<html>not the API</html>")));
  const outcome r = run(&check_net::check_nsclient_web_online, {"url=" + base(server), "command=check_cpu"});
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, r.code) << r.message;
  EXPECT_EQ("Could not parse response from " + base(server), r.message);
}

TEST(CheckNsclientWebOnlineRun, RejectedCredentialsAreCritical) {
  scripted_server server(http_reply(http_response(403, "denied")));
  const outcome r = run(&check_net::check_nsclient_web_online, {"url=" + base(server), "password=wrong"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_EQ("Authentication failed (HTTP 403) on " + base(server), r.message);
}

TEST(CheckNsclientWebOnlineRun, ServerErrorIsCritical) {
  scripted_server server(http_reply(http_response(500, "boom")));
  const outcome r = run(&check_net::check_nsclient_web_online, {"url=" + base(server)});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_EQ("REST API returned HTTP 500 on " + base(server), r.message);
}

TEST(CheckNsclientWebOnlineRun, UnreachableAgentIsCritical) {
  const std::string port = closed_port();
  const outcome r = run(&check_net::check_nsclient_web_online, {"url=http://127.0.0.1:" + port});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code) << r.message;
  EXPECT_TRUE(contains(r.message, "Failed to reach http://127.0.0.1:" + port + ": ")) << r.message;
}
