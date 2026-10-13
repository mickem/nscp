// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Integration tests for Mongoose::Server, Mongoose::ServerImpl and
 * Mongoose::Client.
 *
 * These tests bring up a real HTTP listener on 127.0.0.1, register a small
 * controller and exchange traffic with it. The "raw" round-trip tests use
 * mongoose directly as the client (with a proper poll loop) so they can
 * validate ServerImpl independently of Client::fetch.
 *
 * Note about Client::fetch: it performs a single mg_mgr_poll() call, which
 * is enough to dispatch MG_EV_CONNECT and send the request, but typically
 * not enough to receive the response before mg_mgr_free is called. As a
 * result fetch() commonly returns nullptr even when the server is running
 * correctly. The Client tests below document the current behaviour rather
 * than asserting a successful round-trip.
 *
 * Port collisions: each test reserves its own port offset relative to a
 * pid-derived base to reduce collision risk between concurrent test
 * processes.
 */

#include "Server.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <boost/thread/thread.hpp>
#include <cctype>
#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "MatchController.h"
#include "Request.h"
#include "RequestHandler.h"
#include "Response.h"
#include "StreamResponse.h"

// clang-format off
// Has to be after boost or we get namespace clashes
#include "mongoose_wrapper.h"
// clang-format on

#ifdef _WIN32
#include <process.h>
#define MWT_GETPID _getpid
#else
#include <unistd.h>
#define MWT_GETPID getpid
#endif

using Mongoose::MatchController;
using Mongoose::Request;
using Mongoose::RequestHandlerBase;
using Mongoose::Response;
using Mongoose::Server;
using Mongoose::StreamResponse;
using Mongoose::WebLogger;
using Mongoose::WebLoggerPtr;

namespace {

// ---- Test infrastructure --------------------------------------------------

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
    {
      // The worker-thread tests call one handler from several threads.
      std::lock_guard<std::mutex> g(mu);
      last_method = request.getMethod();
      last_url = request.getUrl();
    }
    auto* r = new StreamResponse(code);
    r->setCode(code, "OK");
    r->append(body);
    return r;
  }
  int code;
  std::string body;
  std::string last_method;
  std::string last_url;
  std::mutex mu;
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
  return 38000 + (pid % 1000);
}

std::string bind_url(const int port) { return "http://127.0.0.1:" + std::to_string(port); }

struct ServerFixture {
  std::shared_ptr<CollectingLogger> logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<Server> server{Server::make_server(logger)};

  void start(int port, MatchController* controller) const {
    server->registerController(controller);  // ownership transferred
    server->start(bind_url(port));
    // Give the polling thread a moment to enter its loop.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
};

// ---- Raw mongoose client used to drive ServerImpl ---------------------------

struct RawResponse {
  int status = 0;
  std::string body;
  std::vector<std::string> set_cookies;
  std::vector<std::pair<std::string, std::string>> headers;
  bool received = false;
  bool error = false;
};

void raw_ev_handler(mg_connection* c, int ev, void* ev_data) {
  auto* out = static_cast<RawResponse*>(c->fn_data);
  if (ev == MG_EV_HTTP_MSG) {
    const auto* hm = static_cast<mg_http_message*>(ev_data);
    out->status = mg_http_status(hm);
    out->body.assign(hm->body.buf, hm->body.len);
    const size_t hmax = std::size(hm->headers);
    for (size_t i = 0; i < hmax && hm->headers[i].name.len > 0; i++) {
      const std::string name(hm->headers[i].name.buf, hm->headers[i].name.len);
      const std::string value(hm->headers[i].value.buf, hm->headers[i].value.len);
      out->headers.emplace_back(name, value);
      if (name.size() == 10 && std::equal(name.begin(), name.end(), "Set-Cookie", [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
          })) {
        out->set_cookies.push_back(value);
      }
    }
    out->received = true;
    c->is_closing = 1;
  } else if (ev == MG_EV_ERROR) {
    out->error = true;
    c->is_closing = 1;
  }
}

// Send a complete HTTP request to the given URL and poll until a response
// arrives or the deadline expires. Unlike Client::fetch this loops on
// mg_mgr_poll until the response is fully received.
RawResponse raw_fetch(const std::string& url, const std::string& request) {
  RawResponse out;
  mg_mgr mgr{};
  mg_mgr_init(&mgr);
  mg_connection* c = mg_http_connect(&mgr, url.c_str(), raw_ev_handler, &out);
  if (c == nullptr) {
    out.error = true;
    mg_mgr_free(&mgr);
    return out;
  }
  mg_send(c, request.c_str(), request.size());

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!out.received && !out.error && std::chrono::steady_clock::now() < deadline) {
    mg_mgr_poll(&mgr, 50);
  }
  mg_mgr_free(&mgr);
  return out;
}

std::string make_get_request(const std::string& path, int port) {
  std::ostringstream oss;
  oss << "GET " << path << " HTTP/1.0\r\n"
      << "Host: 127.0.0.1:" << port << "\r\n"
      << "\r\n";
  return oss.str();
}

}  // namespace

// ---- Server factory / lifecycle (no network) -------------------------------

TEST(Server, MakeServerReturnsNonNull) {
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<Server> server(Server::make_server(logger));
  ASSERT_NE(server, nullptr);
}

TEST(Server, RegisterControllerAndDestructDeletesController) {
  // ServerImpl takes ownership of registered controllers (deletes them in
  // dtor). The test passes by not crashing / leaking.
  auto logger = std::make_shared<CollectingLogger>();
  std::unique_ptr<Server> server(Server::make_server(logger));
  server->registerController(new MatchController());
  server->registerController(new MatchController("/api"));
  SUCCEED();
}

TEST(Server, StopWithoutStartIsSafe) {
  const auto logger = std::make_shared<CollectingLogger>();
  const std::unique_ptr<Server> server(Server::make_server(logger));
  server->stop();
  SUCCEED();
}

TEST(Server, StartThenStopShutsDownCleanly) {
  const int port = choose_port_base() + 10;
  const ServerFixture fx;
  auto* controller = new MatchController();
  fx.start(port, controller);
  fx.server->stop();
  SUCCEED();
}

// ---- ServerImpl behaviour, validated via a raw mongoose client -------------

TEST(ServerImpl, RespondsToRegisteredRoute) {
  const int port = choose_port_base();
  auto* controller = new MatchController();
  auto* handler = new FixedHandler(200, "hello world");
  controller->registerRoute("GET", "/echo", handler);

  const ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/echo", make_get_request("/echo", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received) << "no response from server (error=" << resp.error << ")";
  EXPECT_EQ(resp.status, 200);
  EXPECT_NE(resp.body.find("hello world"), std::string::npos);
  EXPECT_EQ(handler->last_method, "GET");
  EXPECT_EQ(handler->last_url, "/echo");
}

TEST(ServerImpl, ReturnsHttp404ForUnknownRoute) {
  const int port = choose_port_base() + 1;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/known", new FixedHandler(200, "ok"));

  ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/missing", make_get_request("/missing", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 404);
}

TEST(ServerImpl, UnknownRoute404CarriesSecurityHeaders) {
  // The 404 for an unmatched URL is written straight to the wire with
  // mg_http_reply and never builds a Response, so it used to be the one answer
  // that carried none of the hardening headers. A framed 404 is still a framed
  // page, and rest-security-headers.test.ts promises them on every response.
  const int port = choose_port_base() + 21;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/known", new FixedHandler(200, "ok"));

  ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/missing", make_get_request("/missing", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 404);

  auto header_value = [&resp](const std::string& name) {
    for (const auto& kv : resp.headers) {
      if (kv.first.size() == name.size() &&
          std::equal(kv.first.begin(), kv.first.end(), name.begin(),
                     [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); })) {
        return kv.second;
      }
    }
    return std::string();
  };

  const std::string csp = header_value("Content-Security-Policy");
  EXPECT_NE(csp.find("frame-ancestors 'none'"), std::string::npos) << csp;
  EXPECT_EQ(header_value("X-Frame-Options"), "DENY");
  EXPECT_EQ(header_value("X-Content-Type-Options"), "nosniff");
  EXPECT_EQ(header_value("Referrer-Policy"), "no-referrer");
}

// ---- setTlsOptions on a backend that cannot honour it ----------------------

TEST(ServerImpl, SetTlsOptionsIsQuietForAnUnsetSetting) {
  // WEBServer passes an empty `tls version` when the operator never changed it.
  // Logging an error about it would fire on every start and reload of every
  // stock agent using this backend, naming a setting nobody wrote - so the
  // limitation is recorded at debug level instead.
  auto logger = std::make_shared<CollectingLogger>();
  const std::unique_ptr<Server> server(Server::make_server(logger));
  server->setTlsOptions("", "");

  EXPECT_TRUE(logger->errors.empty()) << "unexpected error: " << (logger->errors.empty() ? std::string() : logger->errors.front());
  ASSERT_EQ(logger->debugs.size(), 1u);
  EXPECT_NE(logger->debugs.front().find("no effect"), std::string::npos) << logger->debugs.front();
}

TEST(ServerImpl, SetTlsOptionsWarnsForAnOperatorChosenValue) {
  // An operator who narrowed either setting must learn it did not take effect
  // here, rather than believe the listener was hardened.
  auto logger = std::make_shared<CollectingLogger>();
  const std::unique_ptr<Server> server(Server::make_server(logger));
  server->setTlsOptions("1.3", "ECDHE-RSA-AES256-GCM-SHA384");

  ASSERT_EQ(logger->errors.size(), 2u);
  EXPECT_NE(logger->errors[0].find("tls version = 1.3"), std::string::npos) << logger->errors[0];
  EXPECT_NE(logger->errors[1].find("allowed ciphers"), std::string::npos) << logger->errors[1];
}

TEST(ServerImpl, RespectsHttpVerb) {
  const int port = choose_port_base() + 2;
  auto* controller = new MatchController();
  controller->registerRoute("POST", "/only-post", new FixedHandler(200, "post-ok"));

  const ServerFixture fx;
  fx.start(port, controller);

  // GET to a POST-only route should not match any controller, yielding 404.
  const auto resp = raw_fetch(bind_url(port) + "/only-post", make_get_request("/only-post", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 404);
}

TEST(ServerImpl, MultipleControllersDispatched) {
  // Use two prefix-less controllers so we don't trip MatchController's known
  // prefix-handling inconsistency (handles() strips the prefix but
  // registerRoute() bakes it in, so prefixed routes never match handles()).
  const int port = choose_port_base() + 3;
  auto* first = new MatchController();
  first->registerRoute("GET", "/alpha", new FixedHandler(200, "alpha-body"));
  auto* second = new MatchController();
  second->registerRoute("GET", "/beta", new FixedHandler(200, "beta-body"));

  const ServerFixture fx;
  fx.start(port, first);
  // Register a second controller on the running server.
  fx.server->registerController(second);

  auto a = raw_fetch(bind_url(port) + "/alpha", make_get_request("/alpha", port));
  auto b = raw_fetch(bind_url(port) + "/beta", make_get_request("/beta", port));

  fx.server->stop();

  ASSERT_TRUE(a.received);
  EXPECT_EQ(a.status, 200);
  EXPECT_NE(a.body.find("alpha-body"), std::string::npos);

  ASSERT_TRUE(b.received);
  EXPECT_EQ(b.status, 200);
  EXPECT_NE(b.body.find("beta-body"), std::string::npos);
}

// SameSite=None requires Secure (RFC 6265bis §5.4.7). Browsers drop such a
// cookie if Secure is missing, so the server skips it entirely. These tests
// run over plain HTTP (is_ssl=false), which is precisely the case where the
// guard must trigger.

TEST(ServerImpl, SameSiteNoneOverHttpDropsCookie) {
  const int port = choose_port_base() + 5;
  auto* controller = new MatchController();
  Response::cookie_attrs attrs;
  attrs.same_site = "None";
  attrs.secure = true;  // requested, but is_ssl=false -> Secure won't be emitted
  controller->registerRoute("GET", "/c", new CookieHandler("session", "abc", attrs));

  const ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/c", make_get_request("/c", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 200);
  EXPECT_TRUE(resp.set_cookies.empty()) << "expected no Set-Cookie, got: " << (resp.set_cookies.empty() ? "" : resp.set_cookies.front());
}

TEST(ServerImpl, SameSiteNoneCaseInsensitiveDropsCookie) {
  const int port = choose_port_base() + 6;
  auto* controller = new MatchController();
  Response::cookie_attrs attrs;
  attrs.same_site = "none";  // lowercase
  attrs.secure = true;
  controller->registerRoute("GET", "/c", new CookieHandler("session", "abc", attrs));

  const ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/c", make_get_request("/c", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_TRUE(resp.set_cookies.empty());
}

TEST(ServerImpl, SameSiteNoneWithSecureFalseDropsCookieEvenOnHttps) {
  // Even if the listener were TLS, attrs.secure=false means we will not emit
  // the Secure flag, so the SameSite=None cookie must still be dropped.
  // Validated here over HTTP, which exercises the same guard branch.
  const int port = choose_port_base() + 7;
  auto* controller = new MatchController();
  Response::cookie_attrs attrs;
  attrs.same_site = "None";
  attrs.secure = false;
  controller->registerRoute("GET", "/c", new CookieHandler("session", "abc", attrs));

  const ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/c", make_get_request("/c", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_TRUE(resp.set_cookies.empty());
}

TEST(ServerImpl, SameSiteStrictOverHttpEmitsCookieWithoutSecure) {
  // Sanity check: the guard only targets SameSite=None. SameSite=Strict cookies
  // must still be emitted on plain HTTP, with HttpOnly but without Secure.
  const int port = choose_port_base() + 8;
  auto* controller = new MatchController();
  Response::cookie_attrs attrs;  // defaults: SameSite=Strict, http_only=true, secure=true
  controller->registerRoute("GET", "/c", new CookieHandler("session", "abc", attrs));

  const ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/c", make_get_request("/c", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  ASSERT_EQ(resp.set_cookies.size(), 1u);
  const std::string& sc = resp.set_cookies.front();
  EXPECT_NE(sc.find("session=abc"), std::string::npos) << sc;
  EXPECT_NE(sc.find("HttpOnly"), std::string::npos) << sc;
  EXPECT_NE(sc.find("SameSite=Strict"), std::string::npos) << sc;
  EXPECT_EQ(sc.find("Secure"), std::string::npos) << "Secure must not appear on http: " << sc;
}

TEST(ServerImpl, DoesNotEmitWildcardCors) {
  // Access-Control-Allow-Origin: * was historically emitted on every response.
  // It has been removed because it allowed cross-origin pages to read responses
  // to credential-less authenticated requests. The header must NOT be present.
  const int port = choose_port_base() + 9;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/cors", new FixedHandler(200, "ok"));

  const ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/cors", make_get_request("/cors", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  for (const auto& kv : resp.headers) {
    EXPECT_FALSE(kv.first.size() == 27 &&
                 std::equal(kv.first.begin(), kv.first.end(), "Access-Control-Allow-Origin",
                            [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
        << "wildcard CORS header still present: " << kv.first << ": " << kv.second;
  }
}

TEST(ServerImpl, ReturnsHandlerStatusCode) {
  const int port = choose_port_base() + 4;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/teapot", new FixedHandler(418, "i'm a teapot"));

  const ServerFixture fx;
  fx.start(port, controller);

  auto resp = raw_fetch(bind_url(port) + "/teapot", make_get_request("/teapot", port));

  fx.server->stop();

  ASSERT_TRUE(resp.received);
  EXPECT_EQ(resp.status, 418);
  EXPECT_NE(resp.body.find("teapot"), std::string::npos);
}

// ---- Start / TLS failure reporting -----------------------------------------

TEST(ServerImpl, StartReportsAPortThatIsTaken) {
  const int port = choose_port_base() + 30;
  const ServerFixture first;
  first.start(port, new MatchController());

  const auto logger = std::make_shared<CollectingLogger>();
  const std::unique_ptr<Server> second(Server::make_server(logger));
  // mg_http_listen() returning NULL used to go unnoticed: the caller went on
  // to report a listener that did not exist.
  EXPECT_FALSE(second->start(bind_url(port)));
  first.server->stop();
}

TEST(ServerImpl, SetSslReportsACertificateThatDidNotLoad) {
  const auto logger = std::make_shared<CollectingLogger>();
  const std::unique_ptr<Server> server(Server::make_server(logger));
  std::string cert = "no-such-dir/no-such-certificate.pem";
  std::string key;
  EXPECT_FALSE(server->setSsl(cert, key));
}

// ---- Logging with more than one server --------------------------------------

namespace {
bool logged(CollectingLogger& logger, const std::string& needle) {
  std::lock_guard<std::mutex> g(logger.mu);
  for (const auto* list : {&logger.errors, &logger.infos, &logger.debugs}) {
    for (const std::string& line : *list) {
      if (line.find(needle) != std::string::npos) return true;
    }
  }
  return false;
}

// A request mongoose cannot parse: it logs "HTTP parse" from the poll thread
// of the server that received it.
void send_garbage(const int port) { raw_fetch(bind_url(port), "GET / BOGUS/1.0\r\n\r\n"); }

bool wait_logged(CollectingLogger& logger, const std::string& needle) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    if (logged(logger, needle)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return false;
}
}  // namespace

TEST(ServerImpl, EachServerLogsToItsOwnLogger) {
  // mongoose has one process-wide log hook. It used to be pointed at whichever
  // server was constructed last, so a second server took over the first one's
  // log lines (and the two poll threads shared one line buffer).
  const int port_a = choose_port_base() + 31;
  const int port_b = choose_port_base() + 32;
  const ServerFixture a;
  const ServerFixture b;
  a.start(port_a, new MatchController());
  b.start(port_b, new MatchController());

  send_garbage(port_a);
  EXPECT_TRUE(wait_logged(*a.logger, "HTTP parse"));
  EXPECT_FALSE(logged(*b.logger, "HTTP parse"));
  a.server->stop();
  b.server->stop();
}

TEST(ServerImpl, DestroyingOneServerLeavesTheOthersLoggingOn) {
  // Destroying a server used to clear the hook for every server in the
  // process, and a poll thread could still call into the destroyed logger.
  const int port_a = choose_port_base() + 33;
  const int port_b = choose_port_base() + 34;
  const ServerFixture a;
  {
    ServerFixture b;
    a.start(port_a, new MatchController());
    b.start(port_b, new MatchController());
    b.server->stop();
    b.server.reset();
  }
  send_garbage(port_a);
  EXPECT_TRUE(wait_logged(*a.logger, "HTTP parse"));
  a.server->stop();
}

// ---- Worker threads ---------------------------------------------------------

namespace {
class SlowHandler : public RequestHandlerBase {
 public:
  explicit SlowHandler(std::chrono::milliseconds delay) : delay(delay) {}
  Response* process(Request& /*request*/) override {
    std::this_thread::sleep_for(delay);
    auto* r = new StreamResponse(200);
    r->setCode(200, "OK");
    r->append("slow");
    return r;
  }
  std::chrono::milliseconds delay;
};
}  // namespace

TEST(ServerImpl, WorkerThreadsKeepOtherRequestsMovingPastASlowHandler) {
  // Every handler used to run on the single poll thread, so one slow one
  // stalled every other request and TLS handshake on the server.
  const int port = choose_port_base() + 35;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/slow", new SlowHandler(std::chrono::milliseconds(1500)));
  controller->registerRoute("GET", "/fast", new FixedHandler(200, "fast"));
  const ServerFixture fx;
  fx.server->setWorkerThreads(4);
  fx.start(port, controller);

  RawResponse slow;
  std::thread slow_client([&] { slow = raw_fetch(bind_url(port), make_get_request("/slow", port)); });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  const auto before = std::chrono::steady_clock::now();
  const RawResponse fast = raw_fetch(bind_url(port), make_get_request("/fast", port));
  const auto elapsed = std::chrono::steady_clock::now() - before;
  slow_client.join();
  fx.server->stop();

  ASSERT_TRUE(fast.received);
  EXPECT_EQ(fast.body, "fast");
  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1000);
  ASSERT_TRUE(slow.received);
  EXPECT_EQ(slow.body, "slow");
}

TEST(ServerImpl, WorkerThreadsServeManyConcurrentClients) {
  const int port = choose_port_base() + 36;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/x", new FixedHandler(200, "x"));
  const ServerFixture fx;
  fx.server->setWorkerThreads(4);
  fx.start(port, controller);

  std::atomic<int> ok{0};
  std::vector<std::thread> clients;
  for (int t = 0; t < 8; ++t) {
    clients.emplace_back([&] {
      for (int i = 0; i < 16; ++i) {
        const RawResponse r = raw_fetch(bind_url(port), make_get_request("/x", port));
        if (r.received && r.body == "x") ++ok;
      }
    });
  }
  for (std::thread& c : clients) c.join();
  fx.server->stop();
  EXPECT_EQ(ok.load(), 8 * 16);
}

TEST(ServerImpl, AClientThatLeavesBeforeItsAnswerIsForgotten) {
  // The worker's answer arrives for a connection that has closed; it must be
  // dropped, not kept or written to a stranger.
  const int port = choose_port_base() + 37;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/slow", new SlowHandler(std::chrono::milliseconds(500)));
  controller->registerRoute("GET", "/fast", new FixedHandler(200, "fast"));
  const ServerFixture fx;
  fx.server->setWorkerThreads(2);
  fx.start(port, controller);
  {
    mg_mgr mgr{};
    mg_mgr_init(&mgr);
    RawResponse ignored;
    mg_connection* c = mg_http_connect(&mgr, bind_url(port).c_str(), raw_ev_handler, &ignored);
    ASSERT_NE(c, nullptr);
    const std::string req = make_get_request("/slow", port);
    mg_send(c, req.c_str(), req.size());
    for (int i = 0; i < 4; ++i) mg_mgr_poll(&mgr, 25);
    mg_mgr_free(&mgr);  // hang up before the answer
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(700));
  const RawResponse fast = raw_fetch(bind_url(port), make_get_request("/fast", port));
  fx.server->stop();
  ASSERT_TRUE(fast.received);
  EXPECT_EQ(fast.body, "fast");
}

// ---- TLS that did not load / accept filter / stopping ------------------------

TEST(ServerImpl, StartRefusesAfterAFailedSetSsl) {
  const auto logger = std::make_shared<CollectingLogger>();
  const std::unique_ptr<Server> server(Server::make_server(logger));
  server->registerController(new MatchController());
  std::string cert = "no-such-dir/no-such-certificate.pem";
  std::string key;
  ASSERT_FALSE(server->setSsl(cert, key));
  EXPECT_FALSE(server->start(bind_url(choose_port_base() + 38)));
}

TEST(ServerImpl, AcceptFilterDropsARefusedPeerBeforeAnyRequest) {
  const int port = choose_port_base() + 39;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/x", new FixedHandler(200, "x"));
  const ServerFixture fx;
  std::atomic<int> asked{0};
  fx.server->setAcceptFilter([&asked](const std::string& remote) {
    ++asked;
    return remote != "127.0.0.1";
  });
  fx.start(port, controller);
  const RawResponse r = raw_fetch(bind_url(port), make_get_request("/x", port));
  fx.server->stop();
  EXPECT_FALSE(r.received);
  EXPECT_GE(asked.load(), 1);
}

TEST(ServerImpl, AStoppingServerAcceptsNothingNew) {
  // stop() waits for a running handler; while it does, the listener used to
  // keep accepting requests no worker would ever run.
  const int port = choose_port_base() + 40;
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/slow", new SlowHandler(std::chrono::milliseconds(1500)));
  controller->registerRoute("GET", "/fast", new FixedHandler(200, "fast"));
  const ServerFixture fx;
  fx.server->setWorkerThreads(2);
  fx.start(port, controller);

  RawResponse slow;
  std::thread slow_client([&] { slow = raw_fetch(bind_url(port), make_get_request("/slow", port)); });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::thread stopper([&] { fx.server->stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  const RawResponse late = raw_fetch(bind_url(port), make_get_request("/fast", port));
  stopper.join();
  slow_client.join();

  EXPECT_FALSE(late.received && late.status == 200) << "a request taken while stopping";
  ASSERT_TRUE(slow.received) << "the request in flight is still answered";
  EXPECT_EQ(slow.body, "slow");
}

#ifdef __linux__
#include <dirent.h>
namespace {
int open_descriptors() {
  int count = 0;
  if (DIR* dir = opendir("/proc/self/fd")) {
    while (readdir(dir) != nullptr) ++count;
    closedir(dir);
  }
  return count;
}
}  // namespace

TEST(ServerImpl, AFailedStartLeavesNoSocketsBehind) {
  // The wake-up socket pair used to be created before the listen that
  // failed, and never closed: every reload while the port was taken leaked two.
  const int port = choose_port_base() + 41;
  const ServerFixture holder;
  holder.start(port, new MatchController());
  const int before = open_descriptors();
  for (int i = 0; i < 10; ++i) {
    const auto logger = std::make_shared<CollectingLogger>();
    std::unique_ptr<Server> server(Server::make_server(logger));
    server->setWorkerThreads(4);
    EXPECT_FALSE(server->start(bind_url(port)));
  }
  EXPECT_EQ(open_descriptors(), before);
  holder.server->stop();
}
#endif

// ---- Stopping from a worker / without workers ---------------------------------

namespace {
struct stop_state {
  std::shared_ptr<Server> server;
  std::atomic<bool> released{false};
  std::atomic<bool> finished{false};
  // Written before `released` is set, read after it is seen.
  std::shared_ptr<boost::thread> releaser;
};
// Releases the server it runs on from inside a request - the backstop path of
// a handler whose work ends up stopping its own listener - then keeps going
// for a while, touching itself, the way a handler would.
class ReleaseServerHandler : public RequestHandlerBase {
 public:
  explicit ReleaseServerHandler(std::shared_ptr<stop_state> state) : state_(std::move(state)) {}
  Response* process(Request& /*request*/) override {
    state_->releaser = Mongoose::stop_and_release(state_->server, {});
    state_->released = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // `this` (owned by the server's controller) must still be alive here.
    auto* r = new StreamResponse(200);
    r->setCode(200, "OK");
    r->append(marker_);
    state_->finished = true;
    return r;
  }

 private:
  std::shared_ptr<stop_state> state_;
  std::string marker_ = "released";
};
}  // namespace

TEST(ServerImpl, AHandlerCanReleaseItsOwnServer) {
  // A worker that stops its server used to have the server freed under it
  // (its controller included) while it was still running. stop_and_release()
  // hands the server to a thread of its own, which frees it only after this
  // handler has returned. Meaningful under ASan.
  const int port = choose_port_base() + 42;
  const auto state = std::make_shared<stop_state>();
  auto* controller = new MatchController();
  controller->registerRoute("GET", "/release", new ReleaseServerHandler(state));
  const auto logger = std::make_shared<CollectingLogger>();
  state->server.reset(Server::make_server(logger));
  const std::weak_ptr<Server> watch = state->server;
  state->server->setWorkerThreads(2);
  state->server->registerController(controller);
  ASSERT_TRUE(state->server->start(bind_url(port)));

  raw_fetch(bind_url(port), make_get_request("/release", port));
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!watch.expired() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_TRUE(state->released);
  EXPECT_TRUE(state->finished);
  EXPECT_TRUE(watch.expired()) << "the server is freed once its threads are done";
  // Handed back so an owner can wait for it before unloading the code the
  // controllers' destructors run.
  ASSERT_TRUE(state->releaser);
  state->releaser->join();
}
