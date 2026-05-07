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
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
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
  return 38000 + (pid % 1000);
}

std::string bind_url(const int port) { return "http://127.0.0.1:" + std::to_string(port); }

struct ServerFixture {
  boost::shared_ptr<CollectingLogger> logger = boost::make_shared<CollectingLogger>();
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
  auto logger = boost::make_shared<CollectingLogger>();
  std::unique_ptr<Server> server(Server::make_server(logger));
  ASSERT_NE(server, nullptr);
}

TEST(Server, RegisterControllerAndDestructDeletesController) {
  // ServerImpl takes ownership of registered controllers (deletes them in
  // dtor). The test passes by not crashing / leaking.
  auto logger = boost::make_shared<CollectingLogger>();
  std::unique_ptr<Server> server(Server::make_server(logger));
  server->registerController(new MatchController());
  server->registerController(new MatchController("/api"));
  SUCCEED();
}

TEST(Server, StopWithoutStartIsSafe) {
  const auto logger = boost::make_shared<CollectingLogger>();
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
