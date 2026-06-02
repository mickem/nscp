// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Integration tests for Mongoose::ServerBeastImpl — the Boost.Beast
 * implementation of `Mongoose::Server`. These mirror the assertions in
 * Server_test.cpp but talk to the server via a synchronous Beast client
 * so the suite never depends on mongoose, and exercise `ServerBeastImpl`
 * directly (bypassing the `make_server` factory).
 */

#include "ServerBeastImpl.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "MatchController.h"
#include "Request.h"
#include "RequestHandler.h"
#include "Response.h"
#include "Server.h"
#include "StreamResponse.h"

#ifdef _WIN32
#include <process.h>
#define MWT_GETPID _getpid
#else
#include <unistd.h>
#define MWT_GETPID getpid
#endif

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

using Mongoose::MatchController;
using Mongoose::Request;
using Mongoose::RequestHandlerBase;
using Mongoose::Response;
using Mongoose::ServerBeastImpl;
using Mongoose::StreamResponse;
using Mongoose::WebLogger;
using Mongoose::WebLoggerPtr;

namespace {

class CollectingLogger : public WebLogger {
 public:
  void log_error(const std::string& m) override {
    std::lock_guard<std::mutex> g(mu);
    errors.push_back(m);
  }
  void log_info(const std::string& m) override {
    std::lock_guard<std::mutex> g(mu);
    infos.push_back(m);
  }
  void log_debug(const std::string& m) override {
    std::lock_guard<std::mutex> g(mu);
    debugs.push_back(m);
  }
  std::mutex mu;
  std::vector<std::string> errors;
  std::vector<std::string> infos;
  std::vector<std::string> debugs;
};

class FixedHandler : public RequestHandlerBase {
 public:
  FixedHandler(int code, std::string body) : code(code), body(std::move(body)) {}
  Response* process(Request& request) override {
    last_method = request.getMethod();
    last_url = request.getUrl();
    auto* r = new StreamResponse(code);
    r->setCode(code, "OK");
    r->append(body);
    return r;
  }
  int code;
  std::string body;
  std::string last_method;
  std::string last_url;
};

class CookieHandler : public RequestHandlerBase {
 public:
  CookieHandler(std::string name, std::string value, Response::cookie_attrs attrs) : name(std::move(name)), value(std::move(value)), attrs(std::move(attrs)) {}
  Response* process(Request& /*request*/) override {
    auto* r = new StreamResponse(200);
    r->setCode(200, "OK");
    r->setCookie(name, value, attrs);
    r->append("ok");
    return r;
  }
  std::string name;
  std::string value;
  Response::cookie_attrs attrs;
};

int choose_port_base() {
  const int pid = static_cast<int>(MWT_GETPID());
  // Offset from Server_test.cpp's range (38000+pid%1000) so the two
  // suites don't collide when run in the same process or in parallel.
  return 40000 + (pid % 1000);
}

std::string bind_url(const int port) { return "http://127.0.0.1:" + std::to_string(port); }

struct ServerFixture {
  std::shared_ptr<CollectingLogger> logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server{new ServerBeastImpl(logger)};

  void start(int port, MatchController* controller) const {
    server->registerController(controller);  // ownership transferred
    server->start(bind_url(port));
  }
};

// Synchronous Beast client: opens a TCP connection, sends `req`, reads
// the response, returns the parsed result. Bounded by a per-call timeout
// driven by tcp_stream::expires_after.
struct RawResponse {
  int status = 0;
  std::string body;
  std::vector<std::string> set_cookies;
  std::vector<std::pair<std::string, std::string>> headers;
  bool received = false;
  bool error = false;
};

RawResponse beast_fetch(const std::string& host, int port, const std::string& target) {
  RawResponse out;
  try {
    asio::io_context ioc;
    tcp::resolver resolver(ioc);
    const auto endpoints = resolver.resolve(host, std::to_string(port));
    beast::tcp_stream stream(ioc);
    stream.expires_after(std::chrono::seconds(2));
    stream.connect(endpoints);

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host + ":" + std::to_string(port));
    req.set(http::field::user_agent, "ServerBeastImpl_test/1");
    req.keep_alive(false);

    stream.expires_after(std::chrono::seconds(2));
    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    stream.expires_after(std::chrono::seconds(2));
    http::read(stream, buffer, res);

    out.status = static_cast<int>(res.result_int());
    out.body = res.body();
    for (const auto& f : res) {
      out.headers.emplace_back(std::string(f.name_string()), std::string(f.value()));
      if (f.name() == http::field::set_cookie) {
        out.set_cookies.emplace_back(f.value());
      }
    }
    out.received = true;

    boost::system::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  } catch (const std::exception&) {
    out.error = true;
  }
  return out;
}

// Wait until the server is accepting on `port` (a TCP connect succeeds)
// or the deadline expires. The accept coroutine is spawned via
// asio::spawn and listen() returns synchronously, so this normally
// returns on the first attempt — but the loop keeps the test robust
// against scheduler jitter on busy CI runners.
bool wait_listening(int port, std::chrono::milliseconds deadline) {
  const auto end = std::chrono::steady_clock::now() + deadline;
  while (std::chrono::steady_clock::now() < end) {
    try {
      asio::io_context ioc;
      tcp::socket sock(ioc);
      sock.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
      return true;
    } catch (const std::exception&) {
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
  }
  return false;
}

}  // namespace

// ---- Factory / lifecycle (no network) -------------------------------------

TEST(ServerBeastImpl, ConstructsAndDestructs) {
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  ASSERT_NE(server, nullptr);
}

TEST(ServerBeastImpl, StopWithoutStartIsSafe) {
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  server->stop();
  SUCCEED();
}

TEST(ServerBeastImpl, RegisterControllerAndDestructDeletesController) {
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  server->registerController(new MatchController());
  server->registerController(new MatchController("/api"));
  SUCCEED();  // pass by not leaking / crashing
}

TEST(ServerBeastImpl, StartThenStopShutsDownCleanly) {
  const int port = choose_port_base() + 10;
  const ServerFixture fx;
  auto* controller = new MatchController();
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));
  fx.server->stop();
  SUCCEED();
}

// ---- Routing / dispatch behaviour -----------------------------------------

TEST(ServerBeastImpl, RespondsToRegisteredRoute) {
  const int port = choose_port_base();
  auto* controller = new MatchController();
  auto* handler = new FixedHandler(200, "hello world");
  controller->registerRoute("GET", "/echo", handler);

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/echo");
  fx.server->stop();

  ASSERT_TRUE(resp.received) << "no response (error=" << resp.error << ")";
  EXPECT_EQ(resp.status, 200);
  EXPECT_NE(resp.body.find("hello world"), std::string::npos);
  EXPECT_EQ(handler->last_method, "GET");
  EXPECT_EQ(handler->last_url, "/echo");
}

TEST(ServerBeastImpl, ReturnsHttp404ForUnknownRoute) {
  const int port = choose_port_base() + 1;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/known", new FixedHandler(200, "ok"));

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/missing");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 404);
}

TEST(ServerBeastImpl, RespectsHttpVerb) {
  const int port = choose_port_base() + 2;
  auto* controller = new MatchController();
  controller->registerRoute("POST", "/only-post", new FixedHandler(200, "post-ok"));

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  // GET to a POST-only route should not match any controller -> 404.
  auto resp = beast_fetch("127.0.0.1", port, "/only-post");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 404);
}

TEST(ServerBeastImpl, MultipleControllersDispatched) {
  const int port = choose_port_base() + 3;
  auto* first = new MatchController();
  first->registerRoute("GET", "/alpha", new FixedHandler(200, "alpha-body"));
  auto* second = new MatchController();
  second->registerRoute("GET", "/beta", new FixedHandler(200, "beta-body"));

  const ServerFixture fx;
  fx.start(port, first);
  fx.server->registerController(second);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto a = beast_fetch("127.0.0.1", port, "/alpha");
  auto b = beast_fetch("127.0.0.1", port, "/beta");
  fx.server->stop();

  ASSERT_TRUE(a.received);
  EXPECT_EQ(a.status, 200);
  EXPECT_NE(a.body.find("alpha-body"), std::string::npos);

  ASSERT_TRUE(b.received);
  EXPECT_EQ(b.status, 200);
  EXPECT_NE(b.body.find("beta-body"), std::string::npos);
}

TEST(ServerBeastImpl, ReturnsHandlerStatusCode) {
  const int port = choose_port_base() + 4;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/teapot", new FixedHandler(418, "i'm a teapot"));

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/teapot");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 418);
  EXPECT_NE(resp.body.find("teapot"), std::string::npos);
}

// ---- Cookie / SameSite guard ----------------------------------------------

TEST(ServerBeastImpl, SameSiteNoneOverHttpDropsCookie) {
  const int port = choose_port_base() + 5;
  auto* controller = new MatchController();
  Response::cookie_attrs attrs;
  attrs.same_site = "None";
  attrs.secure = true;  // requested, but is_ssl=false -> Secure won't be emitted, cookie dropped
  controller->registerRoute("GET", "/c", new CookieHandler("session", "abc", attrs));

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/c");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 200);
  EXPECT_TRUE(resp.set_cookies.empty()) << "expected no Set-Cookie, got: " << (resp.set_cookies.empty() ? "" : resp.set_cookies.front());
}

TEST(ServerBeastImpl, SameSiteStrictOverHttpEmitsCookieWithoutSecure) {
  // Sanity check the guard only targets SameSite=None.
  const int port = choose_port_base() + 8;
  auto* controller = new MatchController();
  Response::cookie_attrs attrs;  // defaults: SameSite=Strict, http_only=true, secure=true
  controller->registerRoute("GET", "/c", new CookieHandler("session", "abc", attrs));

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/c");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  ASSERT_EQ(resp.set_cookies.size(), 1u);
  const std::string& sc = resp.set_cookies.front();
  EXPECT_NE(sc.find("session=abc"), std::string::npos) << sc;
  EXPECT_NE(sc.find("HttpOnly"), std::string::npos) << sc;
  EXPECT_NE(sc.find("SameSite=Strict"), std::string::npos) << sc;
  EXPECT_EQ(sc.find("Secure"), std::string::npos) << "Secure must not appear on http: " << sc;
}

TEST(ServerBeastImpl, DoesNotEmitWildcardCors) {
  const int port = choose_port_base() + 9;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/cors", new FixedHandler(200, "ok"));

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/cors");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  for (const auto& kv : resp.headers) {
    EXPECT_FALSE(kv.first.size() == 27 &&
                 std::equal(kv.first.begin(), kv.first.end(), "Access-Control-Allow-Origin",
                            [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
        << "wildcard CORS header still present: " << kv.first << ": " << kv.second;
  }
}

// ---- "No shared state" property (design doc decision 3) -------------------
//
// Two ServerBeastImpl instances on different ports in the same process
// must serve independent traffic — each owns its own io_context, accept
// thread, and controller list, so cross-talk is impossible.

TEST(ServerBeastImpl, TwoInstancesOnDifferentPortsAreIndependent) {
  const int port_a = choose_port_base() + 20;
  const int port_b = choose_port_base() + 21;

  auto logger_a = std::make_shared<CollectingLogger>();
  auto logger_b = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server_a(new ServerBeastImpl(logger_a));
  std::unique_ptr<ServerBeastImpl> server_b(new ServerBeastImpl(logger_b));

  auto* controller_a = new MatchController();
  controller_a->registerRoute("GET", "/", new FixedHandler(200, "from-a"));
  auto* controller_b = new MatchController();
  controller_b->registerRoute("GET", "/", new FixedHandler(200, "from-b"));

  server_a->registerController(controller_a);
  server_b->registerController(controller_b);

  server_a->start(bind_url(port_a));
  server_b->start(bind_url(port_b));
  ASSERT_TRUE(wait_listening(port_a, std::chrono::seconds(2)));
  ASSERT_TRUE(wait_listening(port_b, std::chrono::seconds(2)));

  // Each instance must return its own body. Cross-fetch to prove the
  // routes are not shared.
  auto resp_a = beast_fetch("127.0.0.1", port_a, "/");
  auto resp_b = beast_fetch("127.0.0.1", port_b, "/");

  server_a->stop();
  server_b->stop();

  ASSERT_TRUE(resp_a.received);
  ASSERT_TRUE(resp_b.received);
  EXPECT_EQ(resp_a.status, 200);
  EXPECT_EQ(resp_b.status, 200);
  EXPECT_NE(resp_a.body.find("from-a"), std::string::npos);
  EXPECT_NE(resp_b.body.find("from-b"), std::string::npos);
  // Critically: A's body did NOT leak into B's response and vice versa.
  EXPECT_EQ(resp_a.body.find("from-b"), std::string::npos);
  EXPECT_EQ(resp_b.body.find("from-a"), std::string::npos);
}

// ---- TLS regression: scheme-less bind + setSsl ----------------------------
//
// WEBServer.cpp calls `server->start("0.0.0.0:<port>")` with no scheme
// prefix. Before this regression test, the Beast backend keyed its
// "use TLS" decision off the URL scheme alone, so the server ran
// plain-HTTP, parsed the inbound ClientHello as garbage, and reset the
// socket — exactly what `curl --insecure https://localhost:8443`
// surfaced from a real beast-build deployment. The fix: TLS is on if
// setSsl() loaded a cert/key, regardless of the bind URL scheme. This
// test locks that behaviour in by mirroring WEBServer's call shape.

namespace {

// Self-signed RSA-2048 cert and matching private key, generated with:
//   openssl req -x509 -newkey rsa:2048 -nodes -days 36500 \
//     -subj "/CN=ServerBeastImpl-test" -keyout key.pem -out cert.pem
// Neither the server's own handshake nor the test client (verify_none)
// validates expiry, so the ~100-year window is plenty. If this ever
// needs rotating, regenerate both blobs together (cert + matching key).
constexpr const char* kTestCertPem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDITCCAgmgAwIBAgIUA9ed0e3xtS6fd/T1wN0XrIoJDiAwDQYJKoZIhvcNAQEL\n"
    "BQAwHzEdMBsGA1UEAwwUU2VydmVyQmVhc3RJbXBsLXRlc3QwIBcNMjYwNTIzMTI1\n"
    "OTE1WhgPMjEyNjA0MjkxMjU5MTVaMB8xHTAbBgNVBAMMFFNlcnZlckJlYXN0SW1w\n"
    "bC10ZXN0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1f2DUoVWi9OC\n"
    "1sIJzo62CRHLthttQAU5CJNJsru7MkTwZ0gN0y8i+Kw22GRZdHRwTivaZSNc9Lma\n"
    "vpHkAmypcOUrrrXuJciAimWjgekmSLgkiy1X1I1zA1SLi63+h1r1WU995XcrtwxX\n"
    "b5bsR9eQlp9AL/uV9IcCm8tGNlHCp9HBKIGecBpK4n4fJdxxTVIRdOXTzwvs6vAg\n"
    "b9hn+gnSsS5jUg72opbSPIVoh8TRoqkP65+viP0zMBFFlkrDzEbU/xSnTn/jQowr\n"
    "Ftf5p/QNa7ATe24tfGU0B3z435av5rT7mUZ0Fj1lSiAgYu7lNg5kGgGYQ3YQs5YW\n"
    "4XyoQNuHVQIDAQABo1MwUTAdBgNVHQ4EFgQUe3BAwt81kmydvFRVF+oxs0A8lY4w\n"
    "HwYDVR0jBBgwFoAUe3BAwt81kmydvFRVF+oxs0A8lY4wDwYDVR0TAQH/BAUwAwEB\n"
    "/zANBgkqhkiG9w0BAQsFAAOCAQEAqhA9j1LLuPZh9UhwNrLf9lrbSXjtzXZc6azB\n"
    "Zrw1e5tlESERtmM5qhg/wPcFQr3o2JxdNYhuJ92TakylqHarb/HAW4tRaUPIyMv9\n"
    "RPVcrZN4A2l4c4U2rLhj1Ur1XNNzGDtH63I0vp/h9D1fXpnxGHMradO4X1ZVMgTm\n"
    "hPWhpYBJ125aQQqtbAaVGQjDgAgzfE5ihs5SKadxf0WdlGO9J5EnjnzhbZV0/Pbw\n"
    "gF7j/gwmJujfsSpjmfRafZUZlntDSrFpX0ow1UFPFxjf/uA+p7nNar+/aW+DZaNk\n"
    "YgM9dpdH2PB/stmRZ5H1ys3h0qyQ68wXmbaegYpWWDCgv16Zkw==\n"
    "-----END CERTIFICATE-----\n";

constexpr const char* kTestKeyPem =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDV/YNShVaL04LW\n"
    "wgnOjrYJEcu2G21ABTkIk0myu7syRPBnSA3TLyL4rDbYZFl0dHBOK9plI1z0uZq+\n"
    "keQCbKlw5Suute4lyICKZaOB6SZIuCSLLVfUjXMDVIuLrf6HWvVZT33ldyu3DFdv\n"
    "luxH15CWn0Av+5X0hwKby0Y2UcKn0cEogZ5wGkrifh8l3HFNUhF05dPPC+zq8CBv\n"
    "2Gf6CdKxLmNSDvailtI8hWiHxNGiqQ/rn6+I/TMwEUWWSsPMRtT/FKdOf+NCjCsW\n"
    "1/mn9A1rsBN7bi18ZTQHfPjflq/mtPuZRnQWPWVKICBi7uU2DmQaAZhDdhCzlhbh\n"
    "fKhA24dVAgMBAAECggEAAPA73piSsmN63ZtvB/nco7OK4I4qoq8JlGQ/XLCxqtz3\n"
    "u1KeK02EnYr1ZQOKC0mx1S+G2KflpQ9/Tzo8KFddP6jk0fJR5gm4DpaD6tjtf9oj\n"
    "rUlfBKHOKrb4zOPP2IlPx1Svtg1MvGCQemVmCd0D8RuOE4FkVsGU0FjjqO9SdDWK\n"
    "nmzeTD9ciyDVLv96FsLplXIFneMQy3mHIn2ZQGmznWZ/qbwwQescwYjeDlQZYIFI\n"
    "piIc8Jr9kwQ9NJbrp4dc7oEEIQpWSjeHO9B6fmdlEHoJevUY6DYpuKja3ASeGvkl\n"
    "+i1C8bfh/1yBFPiYylJMoR9yGsnuTFTp9DZBR7C4rQKBgQDy2uYaVy6yZtq1I9kI\n"
    "h+TAvh5bKKArLiHBxyP74SzbORTe6F6K13meODXOBJWhz9G2RAuW4y2OX4XCexfu\n"
    "JvQSwEPA9dYQZo2k0sq3jk/COa7eRD6sSYx7uHT1emaiHCnsYQ9wvN+aXcGIo2j0\n"
    "EUvCPD45SZdWVRNN1Z1oTXQt5wKBgQDhkqbO0RLL41eZ5w8W93xUIukWCWX7sGeR\n"
    "xXIzyrKK4rWyhfgLJDcli4FQwy4GIhA4gPQ7R/0XAHSC5xEgEXGz7w1DSiylpoiL\n"
    "e7ZnQomCttBBVatCv+DL4o9M5+FhWee9sa2NbrheCPX7FpFgvNHRlyY4G91nS8tk\n"
    "0IpedoUhYwKBgQCMC+G/9OCv7pJW/SouOjeXUsuso/vhisPavF0q5op4jS1U8kl6\n"
    "5ZFzxVR5zrj/TBnScuEADVf7D2jSYyvEoWAE5CzuPJZKdOlf0FMokP+7sIoAEPjX\n"
    "X76Mpi2EViaTe4xNjRdbWv/TRBfUFO/0N3kptJXpcV+9YGg2bWZNfMTvpwKBgEcw\n"
    "bDp7Gy6DiiMFG+sIohE/j0YoIypiit0jbh5QSzavw94aj6Scglb0BxTA9GZ1G59p\n"
    "eyq2VnaK6zpgyDPRrYu21v20jBCfVRqIKZG/GvzIy0LDUbBYNA7Eaqs/xw5dPFjO\n"
    "mVI4bjEnLNJYVfssvB8kT6iHisFN11vywKh9SRi3AoGAYrES1oRc7rD9m7DZu4yU\n"
    "BmXPTh9JSEAxhV4r0aAjApKpxbz7KLQGmBezu9yUrqq7vJr7CBeYUj+pqril2kir\n"
    "wWILLhJacKSpsPQajh8/NCPerK80Y2FLW6DfvWHAOlJFAuJdNLG5zPECzV+vhCEx\n"
    "6qZZpGuERdmAAd9Puhm/zc8=\n"
    "-----END PRIVATE KEY-----\n";

// Write a string to `path`; returns false (and the test fails) on I/O error.
bool write_pem(const std::string& path, const char* content) {
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f << content;
  return f.good();
}

// Synchronous TLS Beast client that doesn't verify the server cert —
// the test cert is self-signed and we're only proving the handshake
// completes and the response body round-trips.
RawResponse beast_fetch_tls(const std::string& host, int port, const std::string& target) {
  RawResponse out;
  try {
    asio::io_context ioc;
    asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
    ctx.set_verify_mode(asio::ssl::verify_none);

    tcp::resolver resolver(ioc);
    const auto endpoints = resolver.resolve(host, std::to_string(port));
    asio::ssl::stream<beast::tcp_stream> stream(beast::tcp_stream(ioc), ctx);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(2));
    beast::get_lowest_layer(stream).connect(endpoints);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(2));
    stream.handshake(asio::ssl::stream_base::client);

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host + ":" + std::to_string(port));
    req.set(http::field::user_agent, "ServerBeastImpl_test/1");
    req.keep_alive(false);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(2));
    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(2));
    http::read(stream, buffer, res);
    out.status = static_cast<int>(res.result_int());
    out.body = res.body();
    out.received = true;

    boost::system::error_code ec;
    stream.shutdown(ec);  // best-effort graceful close
  } catch (const std::exception&) {
    out.error = true;
  }
  return out;
}

}  // namespace

TEST(ServerBeastImpl, SchemeLessBindWithSslCertEnablesTls) {
  const int port = choose_port_base() + 22;

  // Write the embedded test PEMs to disk; setSsl() takes file paths
  // (matching the WEBServer call shape we're regressing).
  const std::string scratch = (std::filesystem::temp_directory_path() / ("server_beast_impl_test_" + std::to_string(MWT_GETPID()))).string();
  std::error_code mkdir_ec;
  std::filesystem::create_directories(scratch, mkdir_ec);
  const std::string cert_path = scratch + "/cert.pem";
  const std::string key_path = scratch + "/key.pem";
  ASSERT_TRUE(write_pem(cert_path, kTestCertPem)) << "failed to write " << cert_path;
  ASSERT_TRUE(write_pem(key_path, kTestKeyPem)) << "failed to write " << key_path;

  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/", new FixedHandler(200, "hello-tls"));
  server->registerController(controller);

  std::string cert_path_copy = cert_path;  // setSsl takes non-const refs
  std::string key_path_copy = key_path;
  server->setSsl(cert_path_copy, key_path_copy);

  // Critically: bind with NO scheme prefix — same call shape as
  // WEBServer.cpp:316 ("server->start(\"0.0.0.0:\" + port)"). Before the
  // fix this would have left use_tls_=false, plain-HTTP-parsing the
  // ClientHello and resetting the connection.
  server->start("127.0.0.1:" + std::to_string(port));
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch_tls("127.0.0.1", port, "/");
  server->stop();

  // Clean up the temp dir so concurrent test runs don't pile up.
  std::filesystem::remove_all(scratch, mkdir_ec);

  ASSERT_TRUE(resp.received) << "TLS handshake failed (logger errors: "
                              << (logger->errors.empty() ? std::string("(none)") : logger->errors.front()) << ")";
  EXPECT_EQ(resp.status, 200);
  EXPECT_NE(resp.body.find("hello-tls"), std::string::npos);
}

// ---- Hardening: port validation, body limit, header injection, misuse ----
//
// Each test below corresponds to a fix landed in the same change:
//   * port range validation in parse_bind
//   * CRLF skip in mongoose_to_beast for controller-supplied headers
//   * body_limit_ wired through setBodyLimit() and the request_parser
//   * already-started guard on start()
//   * setSsl() / setBodyLimit() after start() get rejected with a logged
//     warning instead of silently mismatching the live state
//   * registerController() is safe to call after start() (mutex + snapshot)

namespace {

// Handler that injects an X-Evil header containing a literal CRLF —
// bypassing Response::setHeader() (which strips CRLF at the source) by
// mutating the headers map directly. This exercises mongoose_to_beast's
// defense-in-depth skip; if anyone ever weakens or removes that layer,
// the test will catch a forged X-Injected header reaching the client.
class CrlfHeaderHandler : public RequestHandlerBase {
 public:
  Response* process(Request& /*request*/) override {
    auto* r = new StreamResponse(200);
    r->setCode(200, "OK");
    r->get_headers()["X-Evil"] = "innocent\r\nX-Injected: forged";
    r->append("ok");
    return r;
  }
};

// Echo back the request body length, so the test can prove the request
// was either parsed or rejected based on size.
class EchoBodySizeHandler : public RequestHandlerBase {
 public:
  Response* process(Request& request) override {
    auto* r = new StreamResponse(200);
    r->setCode(200, "OK");
    r->append("body=" + std::to_string(request.getData().size()));
    return r;
  }
};

// Same beast_fetch as above but sends a POST with a body of `body_size`.
RawResponse beast_post(const std::string& host, int port, std::size_t body_size) {
  RawResponse out;
  try {
    asio::io_context ioc;
    tcp::resolver resolver(ioc);
    const auto endpoints = resolver.resolve(host, std::to_string(port));
    beast::tcp_stream stream(ioc);
    stream.expires_after(std::chrono::seconds(2));
    stream.connect(endpoints);

    http::request<http::string_body> req{http::verb::post, "/", 11};
    req.set(http::field::host, host + ":" + std::to_string(port));
    req.set(http::field::content_type, "application/octet-stream");
    req.body() = std::string(body_size, 'x');
    req.prepare_payload();
    req.keep_alive(false);

    stream.expires_after(std::chrono::seconds(2));
    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    stream.expires_after(std::chrono::seconds(2));
    http::read(stream, buffer, res);
    out.status = static_cast<int>(res.result_int());
    out.body = res.body();
    out.received = true;
    boost::system::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  } catch (const std::exception&) {
    out.error = true;
  }
  return out;
}

}  // namespace

TEST(ServerBeastImpl, PortOutOfRangeIsRejectedAtStart) {
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  // Use an out-of-range port whose low 16 bits land on a (likely free) port
  // from our usual range, so the "did it bind the truncated value?" probe
  // below can't collide with an unrelated listener on a fixed port.
  // Historically a value like 99999 truncated to (99999 & 0xFFFF) and bound
  // silently; now it must be refused and logged.
  const int truncated = choose_port_base() + 40;  // within the 40000+ test range
  const long oob_port = 0x10000L + truncated;      // > 65535 -> rejected; (oob_port & 0xFFFF) == truncated
  server->start("127.0.0.1:" + std::to_string(oob_port));
  EXPECT_FALSE(wait_listening(truncated, std::chrono::milliseconds(200))) << "server bound truncated port — guard regressed";
  ASSERT_FALSE(logger->errors.empty());
  EXPECT_NE(logger->errors.front().find("out of range"), std::string::npos) << logger->errors.front();
}

TEST(ServerBeastImpl, MalformedPortIsRejectedAtStart) {
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  server->start("127.0.0.1:abc");
  ASSERT_FALSE(logger->errors.empty());
  EXPECT_NE(logger->errors.front().find("Invalid port"), std::string::npos) << logger->errors.front();
}

TEST(ServerBeastImpl, StrippableTrailingGarbageInPortIsRejected) {
  // "8080abc" used to silently parse as 8080 (stoul stops at the first
  // non-digit). Now we require the whole port substring to be numeric.
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  server->start("127.0.0.1:8080abc");
  ASSERT_FALSE(logger->errors.empty());
  EXPECT_NE(logger->errors.front().find("Invalid port"), std::string::npos) << logger->errors.front();
}

TEST(ServerBeastImpl, HeaderCrlfFromControllerIsDropped) {
  const int port = choose_port_base() + 30;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/h", new CrlfHeaderHandler());

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/h");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 200);
  // The controller asked for X-Evil; the server must have dropped it
  // (both halves of the pair contain CR/LF on the value side). Critically
  // X-Injected must NOT appear as a real header — that would be HTTP
  // response splitting.
  for (const auto& kv : resp.headers) {
    EXPECT_NE(kv.first, "X-Injected") << "response splitting succeeded: " << kv.first << ": " << kv.second;
    EXPECT_NE(kv.first, "X-Evil") << "X-Evil should have been suppressed due to CRLF in value";
  }
}

TEST(ServerBeastImpl, BodyLimitIsEnforced) {
  const int port = choose_port_base() + 31;
  auto* controller = new MatchController();
  controller->registerRoute("POST", "/", new EchoBodySizeHandler());

  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<ServerBeastImpl> server(new ServerBeastImpl(logger));
  server->registerController(controller);
  // Tiny limit so we don't have to send megabytes.
  server->setBodyLimit(1024);
  server->start(bind_url(port));
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  // 256 bytes < limit -> accepted.
  auto ok = beast_post("127.0.0.1", port, 256);
  // 4 KiB > limit -> Beast's parser aborts the read; the coroutine
  // returns before writing anything, so the client either reads nothing
  // or the connection is closed mid-stream. Both manifest as
  // `received == false` or a non-2xx status (depending on timing).
  auto too_big = beast_post("127.0.0.1", port, 4096);

  server->stop();

  ASSERT_TRUE(ok.received);
  EXPECT_EQ(ok.status, 200);
  EXPECT_NE(ok.body.find("body=256"), std::string::npos);
  EXPECT_FALSE(too_big.received && too_big.status == 200 && too_big.body.find("body=4096") != std::string::npos)
      << "oversized body slipped past the body_limit guard (received=" << too_big.received << ", status=" << too_big.status << ", body=" << too_big.body << ")";
}

TEST(ServerBeastImpl, SecondStartIsRejected) {
  const int port = choose_port_base() + 32;
  const ServerFixture fx;
  auto* controller = new MatchController();
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  // Second start() on the same instance: the ioc_ was never stop()'d
  // so this is a true double-start. It must refuse and log without
  // throwing or deadlocking.
  fx.server->start(bind_url(port + 1));
  fx.server->stop();

  ASSERT_FALSE(fx.logger->errors.empty());
  bool found = false;
  for (const auto& e : fx.logger->errors) {
    if (e.find("already-started") != std::string::npos) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "expected 'already-started' log, got: " << fx.logger->errors.front();
}

TEST(ServerBeastImpl, RestartAfterStopServesAgain) {
  // A plugin can be unloaded and reloaded, so start -> stop -> start must
  // work: each start() re-initializes the io_context run state (restart(),
  // re-arm work guard, clear stopping_) so the restarted server accepts again.
  const int port = choose_port_base() + 36;
  const ServerFixture fx;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/", new FixedHandler(200, "ok"));

  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));
  auto first = beast_fetch("127.0.0.1", port, "/");
  ASSERT_TRUE(first.received);
  EXPECT_EQ(first.status, 200);

  fx.server->stop();
  EXPECT_FALSE(wait_listening(port, std::chrono::milliseconds(300))) << "still listening after stop()";

  // Restart the same instance — it must bind and serve again.
  fx.server->start(bind_url(port));
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2))) << "server did not accept after restart";
  auto second = beast_fetch("127.0.0.1", port, "/");
  fx.server->stop();

  ASSERT_TRUE(second.received) << "no response after restart";
  EXPECT_EQ(second.status, 200);
  EXPECT_NE(second.body.find("ok"), std::string::npos);
}

TEST(ServerBeastImpl, CookieAttributeCrlfIsDropped) {
  // Response::setCookie stores path/same_site verbatim; a controller must not
  // be able to smuggle CR/LF through them into the Set-Cookie header (HTTP
  // response splitting). The whole cookie should be dropped.
  const int port = choose_port_base() + 37;
  auto* controller = new MatchController();
  Response::cookie_attrs attrs;
  attrs.path = "/\r\nX-Injected: forged";
  controller->registerRoute("GET", "/c", new CookieHandler("session", "abc", attrs));

  const ServerFixture fx;
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  auto resp = beast_fetch("127.0.0.1", port, "/c");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 200);
  EXPECT_TRUE(resp.set_cookies.empty()) << "cookie with CRLF in path should have been dropped";
  for (const auto& kv : resp.headers) {
    EXPECT_NE(kv.first, "X-Injected") << "response splitting via cookie path: " << kv.first << ": " << kv.second;
  }
}

TEST(ServerBeastImpl, SetSslAfterStartIsRejected) {
  const int port = choose_port_base() + 33;
  const ServerFixture fx;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/", new FixedHandler(200, "ok"));
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  // Trying to set SSL on a running server has no path to take effect
  // (ssl_ctx_ was built at start). The implementation logs and bails
  // rather than silently misleading the caller.
  std::string cert = "irrelevant";
  std::string key = "irrelevant";
  fx.server->setSsl(cert, key);

  fx.server->stop();

  bool found = false;
  for (const auto& e : fx.logger->errors) {
    if (e.find("setSsl()") != std::string::npos && e.find("after start") != std::string::npos) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "expected 'setSsl after start' warning in log";
}

TEST(ServerBeastImpl, SetBodyLimitAfterStartIsRejected) {
  const int port = choose_port_base() + 34;
  const ServerFixture fx;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/", new FixedHandler(200, "ok"));
  fx.start(port, controller);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  fx.server->setBodyLimit(1);  // Would otherwise reject any non-empty body.

  // Body limit didn't change — a request with no body still gets through.
  auto resp = beast_fetch("127.0.0.1", port, "/");
  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 200);
  bool found = false;
  for (const auto& e : fx.logger->errors) {
    if (e.find("setBodyLimit()") != std::string::npos && e.find("after start") != std::string::npos) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "expected 'setBodyLimit after start' warning in log";
}

TEST(ServerBeastImpl, RegisterControllerAfterStartIsThreadSafe) {
  // Locks the snapshot-on-read pattern in dispatch() against the mutex
  // taken by registerController(). Stress with concurrent registrations
  // while requests are in flight; the test passes if no ASan/UB-san
  // report fires and every request gets a sensible response.
  const int port = choose_port_base() + 35;
  const ServerFixture fx;
  auto* initial = new MatchController();
  initial->registerRoute("GET", "/initial", new FixedHandler(200, "initial-body"));
  fx.start(port, initial);
  ASSERT_TRUE(wait_listening(port, std::chrono::seconds(2)));

  std::atomic<bool> done{false};
  std::thread registrar([&] {
    for (int i = 0; i < 64 && !done; ++i) {
      auto* c = new MatchController();
      c->registerRoute("GET", "/" + std::to_string(i), new FixedHandler(200, "late-" + std::to_string(i)));
      fx.server->registerController(c);
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  });

  // Fire requests against the always-present route while registrations
  // race in. Each must come back 200 with the right body.
  for (int i = 0; i < 32; ++i) {
    auto r = beast_fetch("127.0.0.1", port, "/initial");
    ASSERT_TRUE(r.received) << "request " << i << " dropped";
    EXPECT_EQ(r.status, 200);
    EXPECT_NE(r.body.find("initial-body"), std::string::npos);
  }
  done = true;
  registrar.join();
  fx.server->stop();
}
