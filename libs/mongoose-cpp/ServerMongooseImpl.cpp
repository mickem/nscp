// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ServerMongooseImpl.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <memory>
#include <mutex>
#include <nsclient/nsclient_exception.hpp>
#include <sstream>
#include <string>
#include <threads/guarded_thread.hpp>
#include <utility>

#include "Helpers.h"
#include "StreamResponse.h"
#include "cert_loader.h"

using namespace std;
using namespace Mongoose;

boost::posix_time::ptime now() { return boost::get_system_time(); }

namespace {
// mongoose has one log hook for the whole process, and more than one server
// can run in it (the WEB server and the NCPA server, each with its own poll
// thread). So the hook is installed once and never cleared, and each line is
// routed by the thread that produced it: every thread that drives a server's
// mongoose manager marks itself with that server's target for as long as it
// does. A line from a thread that is not driving any server has nowhere to go
// and is dropped, which is what clearing the hook used to do.
thread_local ServerMongooseImpl::log_target *tl_log_target = nullptr;
// The line being assembled: mongoose hands the hook one character at a time,
// and two poll threads writing one shared buffer would corrupt it.
thread_local std::string tl_log_line;

struct scoped_log_target {
  ServerMongooseImpl::log_target *previous;
  explicit scoped_log_target(ServerMongooseImpl::log_target *target) : previous(tl_log_target) { tl_log_target = target; }
  ~scoped_log_target() { tl_log_target = previous; }
  scoped_log_target(const scoped_log_target &) = delete;
  scoped_log_target &operator=(const scoped_log_target &) = delete;
};

void log_wrapper(char c, void *) {
  if (c != '\n' && c != '\r') {
    tl_log_line += c;
    return;
  }
  if (tl_log_line.empty()) {
    return;
  }
  std::string line;
  line.swap(tl_log_line);
  ServerMongooseImpl::log_target *target = tl_log_target;
  if (target == nullptr || target->logger == nullptr) {
    return;
  }
  if (boost::algorithm::contains(line, "alert certificate unknown")) {
    if (!target->cert_issue_logged.exchange(true)) {
      target->logger->log_error("This could be due to self-signed certificates: " + line);
    }
  } else if (boost::algorithm::contains(line, ":error:")) {
    target->logger->log_error(line);
  } else {
    target->logger->log_info(line);
  }
}

std::once_flag log_hook_installed;
void install_log_hook() {
  std::call_once(log_hook_installed, [] {
    mg_log_set_fn(&log_wrapper, nullptr);
    mg_log_set(MG_LL_ERROR);
  });
}
}  // namespace

namespace Mongoose {
ServerMongooseImpl::ServerMongooseImpl(WebLoggerPtr logger) : logger_(std::move(logger)), stop_thread_(false) {
  install_log_hook();
  log_target_.logger = logger_.get();
  const scoped_log_target route(&log_target_);
  memset(&mgr, 0, sizeof(mg_mgr));
  mg_mgr_init(&mgr);
}

ServerMongooseImpl::~ServerMongooseImpl() {
  ServerMongooseImpl::stop();
  if (!poll_thread_owns_mgr_) {
    // Never started (or the start failed): nothing else will release what
    // mg_mgr_init() and a failed listen opened.
    const scoped_log_target route(&log_target_);
    mg_mgr_free(&mgr);
  }

  for (const auto &controller : controllers) {
    delete controller;
  }
  controllers.clear();
}

bool ServerMongooseImpl::setSsl(std::string &new_certificate, std::string &new_key) {
#if MG_ENABLE_OPENSSL
  try {
    auto cert_and_key = cert_loader::load_certificates(new_certificate, new_key);
    certificate = cert_and_key.first;
    key = cert_and_key.second;
  } catch (const nsclient::nsclient_exception &e) {
    logger_->log_error("Failed to load certificates: " + e.reason());
    certificate.clear();
    key.clear();
    ssl_failed_ = true;
    return false;
  }
  ssl_failed_ = certificate.empty() || key.empty();
  return !ssl_failed_;
#else
  logger_->log_error("Not compiled with TLS");
  ssl_failed_ = true;
  return false;
#endif
}

void ServerMongooseImpl::setWorkerThreads(const std::size_t threads) {
  if (thread_) {
    logger_->log_error("setWorkerThreads() called after start() — ignored; restart the server to apply");
    return;
  }
  worker_threads_ = threads == 0 ? 1 : threads;
}

void ServerMongooseImpl::setAcceptFilter(accept_filter filter) {
  if (thread_) {
    logger_->log_error("setAcceptFilter() called after start() — ignored; restart the server to apply");
    return;
  }
  accept_filter_ = std::move(filter);
}

void ServerMongooseImpl::setTlsOptions(const std::string &tls_version, const std::string &ciphers) {
  // mongoose drives TLS through its own stack, which exposes neither a
  // protocol-version range nor a cipher list. Saying so is the point: an
  // operator who narrowed either setting must know it did not take effect on
  // this backend rather than believe the listener was hardened.
  //
  // Only for a value the operator chose, though. WEBServer passes an empty
  // `tls version` when the setting is still at its default, because an error
  // on every start and reload of every stock Windows agent - naming a setting
  // nobody wrote - is noise that teaches operators to ignore the log. The
  // backend's limitation is still recorded, at debug level.
  if (tls_version.empty() && ciphers.empty()) {
    logger_->log_debug("The mongoose web backend drives TLS through its own stack: 'tls version' and 'allowed ciphers' have no effect here.");
    return;
  }
  if (!tls_version.empty()) {
    logger_->log_error("Ignoring 'tls version = " + tls_version + "': the mongoose web backend does not expose the TLS protocol version.");
  }
  if (!ciphers.empty()) {
    logger_->log_error("Ignoring 'allowed ciphers': the mongoose web backend does not expose the TLS cipher list.");
  }
}

void ServerMongooseImpl::thread_proc() {
  const scoped_log_target route(&log_target_);
  while (true) {
    mg_mgr_poll(&mgr, 1000);
    if (stop_thread_) {
      // Write out what is already answered before closing everything: stop()
      // has drained the workers, and their last answers may still be waiting
      // for this thread, or sitting in a send buffer. mg_mgr_free() closes
      // connections without flushing them. Bounded, so a client that never
      // reads cannot hold a reload up.
      for (int i = 0; i < 40 && has_unsent_answers(); ++i) {
        mg_mgr_poll(&mgr, 50);
      }
      mg_mgr_free(&mgr);
      return;
    }
  }
}

bool ServerMongooseImpl::has_unsent_answers() const {
  if (ready_replies_ > 0) {
    return true;
  }
  for (const mg_connection *c = mgr.conns; c != nullptr; c = c->next) {
    if (!c->is_listening && !c->is_closing && c->send.len > 0) {
      return true;
    }
  }
  return false;
}

bool ServerMongooseImpl::start(const std::string &bind) {
  const scoped_log_target route(&log_target_);
  if (thread_) {
    logger_->log_error("start() called on an already-started server — ignored");
    return false;
  }
  if (ssl_failed_) {
    // See ServerBeastImpl::start(): never fall back to cleartext.
    logger_->log_error("TLS was requested for " + bind +
                       " but the certificate could not be loaded; refusing to serve plain HTTP. The listener has NOT been started.");
    return false;
  }
  // Listen first: the wake-up socket pair is only created once there is
  // something to wake, so a start that fails (the port is taken) leaves
  // nothing open behind it - a reload loop would otherwise leak two sockets
  // per attempt.
  if (mg_http_listen(&mgr, bind.c_str(), event_handler, this) == nullptr) {
    logger_->log_error("Failed to listen on " + bind + ". The listener has NOT been started.");
    return false;
  }
  use_workers_ = worker_threads_ > 1;
  if (use_workers_ && !mg_wakeup_init(&mgr)) {
    logger_->log_error("Failed to set up the request worker pool; answering every request on the poll thread instead.");
    use_workers_ = false;
  }
  accepting_ = true;
  const WebLoggerPtr log = logger_;
  if (use_workers_) {
    {
      const std::lock_guard<std::mutex> lock(jobs_mutex_);
      stop_workers_ = false;
    }
    for (std::size_t i = 0; i < worker_threads_; ++i) {
      workers_.push_back(
          threads::start_guarded_thread("web server worker", [this] { worker_proc(); }, [log](const std::string &message) { log->log_error(message); }));
    }
  }
  thread_ = threads::start_guarded_thread("web server", [this] { thread_proc(); }, [log](const std::string &message) { log->log_error(message); });
  poll_thread_owns_mgr_ = true;
  return true;
}

void ServerMongooseImpl::stop() {
  // Stop taking work first: from here a new connection is closed as it is
  // accepted and a new request is answered 503 (see event_handler), so a
  // reload waiting on a slow check does not keep accepting polls no worker
  // will run. Then the workers, while the poll thread still runs to write
  // their answers out - queued requests get a 503, one already running is
  // waited for. The poll thread goes last: mg_wakeup() needs its manager.
  accepting_ = false;
  if (!workers_.empty()) {
    std::deque<job> abandoned;
    {
      const std::lock_guard<std::mutex> lock(jobs_mutex_);
      stop_workers_ = true;
      abandoned.swap(jobs_);
    }
    jobs_cv_.notify_all();
    for (const job &queued : abandoned) {
      StreamResponse unavailable;
      unavailable.setCode(HTTP_SERVICE_UNAVAILABLE, REASON_SERVICE_UNAVAILABLE);
      unavailable.append("Server is stopping");
      hand_back(queued, render(unavailable, queued.is_ssl));
    }
    const bool from_worker = std::any_of(workers_.begin(), workers_.end(),
                                         [](const std::shared_ptr<boost::thread> &worker) { return worker->get_id() == boost::this_thread::get_id(); });
    for (const std::shared_ptr<boost::thread> &worker : workers_) {
      if (from_worker) {
        worker->detach();
      } else {
        worker->join();
      }
    }
    workers_.clear();
  }
  if (thread_) {
    stop_thread_ = true;
    // Stopping the server from the thread that runs it: a request handler took
    // a route that unloads the module (the core refuses that, this is the
    // backstop for any other path). Joining here would join this thread with
    // itself, which throws rather than returning, and the throw would escape a
    // destructor further up. Let it go instead - the poll loop sees the stop
    // flag and returns as soon as this handler does.
    if (thread_->get_id() == boost::this_thread::get_id()) {
      thread_->detach();
      thread_.reset();
      return;
    }
    thread_->interrupt();
    thread_->join();
  }
  thread_.reset();
}

void ServerMongooseImpl::registerController(Controller *controller) { controllers.push_back(controller); }

void ServerMongooseImpl::worker_proc() {
  while (true) {
    job current;
    {
      std::unique_lock<std::mutex> lock(jobs_mutex_);
      jobs_cv_.wait(lock, [this] { return stop_workers_ || !jobs_.empty(); });
      if (stop_workers_) {
        return;
      }
      current = std::move(jobs_.front());
      jobs_.pop_front();
    }
    reply answer;
    try {
      const std::unique_ptr<Response> response(current.controller->handleRequest(*current.request));
      answer = render(*response, current.is_ssl);
    } catch (const std::exception &e) {
      const std::unique_ptr<Response> response(Controller::internalErrorFromException(e.what()));
      answer = render(*response, current.is_ssl);
    } catch (...) {
      const std::unique_ptr<Response> response(Controller::internalErrorFromException("Unknown error"));
      answer = render(*response, current.is_ssl);
    }
    hand_back(current, std::move(answer));
  }
}

void ServerMongooseImpl::hand_back(const job &finished, reply answer) {
  {
    const std::lock_guard<std::mutex> lock(jobs_mutex_);
    // Closed while we worked: nobody to answer.
    if (waiting_.erase(finished.connection_id) == 0) {
      return;
    }
    pending_reply &pending = replies_[finished.connection_id];
    pending.answer = std::move(answer);
    pending.close = finished.close;
    ++ready_replies_;
  }
  // A nudge, not the delivery guarantee: mg_wakeup() reports success even
  // when its datagram is dropped, so the poll thread also sweeps on
  // MG_EV_POLL (event_handler) while ready_replies_ is non-zero.
  mg_wakeup(&mgr, finished.connection_id, "r", 1);
}

void ServerMongooseImpl::deliver(mg_connection *connection) {
  pending_reply pending;
  {
    const std::lock_guard<std::mutex> lock(jobs_mutex_);
    const auto it = replies_.find(connection->id);
    if (it == replies_.end()) {
      return;
    }
    pending = std::move(it->second);
    replies_.erase(it);
    --ready_replies_;
  }
  mg_http_reply(connection, pending.answer.code, pending.answer.headers.c_str(), "%s", pending.answer.body.c_str());
  if (pending.close) {
    connection->is_draining = 1;
  }
}

void ServerMongooseImpl::forget(const unsigned long connection_id) {
  const std::lock_guard<std::mutex> lock(jobs_mutex_);
  waiting_.erase(connection_id);
  if (replies_.erase(connection_id) > 0) --ready_replies_;
}

bool ServerMongooseImpl::admits(mg_connection *connection) const {
  if (!accept_filter_) {
    return true;
  }
  char buf[100];
  mg_snprintf(buf, sizeof(buf), "%M", mg_print_ip, &connection->rem);
  return accept_filter_(std::string(buf));
}

void ServerMongooseImpl::event_handler(mg_connection *connection, int ev, void *ev_data) {
  if (connection->fn_data != nullptr) {
    auto *impl = static_cast<ServerMongooseImpl *>(connection->fn_data);
    if (ev == MG_EV_WAKEUP || (ev == MG_EV_POLL && impl->ready_replies_ > 0)) {
      impl->deliver(connection);
    }
    if (ev == MG_EV_CLOSE && impl->use_workers_) {
      impl->forget(connection->id);
    }
    if (ev == MG_EV_ACCEPT) {
      if (!impl->accepting_ || !impl->admits(connection)) {
        // Before the TLS handshake: a refused or late peer costs nothing more.
        connection->is_closing = 1;
        return;
      }
#if MG_ENABLE_OPENSSL
      impl->initTls(connection);
#else
      impl->logger_->log_error("Not compiled with TLS support");
#endif
    }
    if (ev == MG_EV_HTTP_MSG) {
      if (!impl->accepting_) {
        // A keep-alive connection asking for more while the server stops.
        mg_http_reply(connection, HTTP_SERVICE_UNAVAILABLE, "Content-Type: text/plain\r\nConnection: close\r\n", "Server is stopping");
        connection->is_draining = 1;
        return;
      }
      auto message = static_cast<struct mg_http_message *>(ev_data);
      impl->onHttpRequest(connection, message);
    }
  }
}
#if MG_ENABLE_OPENSSL
void ServerMongooseImpl::initTls(mg_connection *connection) const {
  if (certificate.empty() || key.empty()) {
    return;
  }
  mg_tls_opts opts{};
  memset(&opts, 0, sizeof(mg_tls_opts));
  // TODO: Should we add name?
  opts.cert = mg_str(certificate.c_str());
  opts.key = mg_str(key.c_str());
  mg_tls_init(connection, &opts);
}
#endif

Request build_request(const std::string &ip, const mg_http_message *message, const bool is_ssl, const std::string &method) {
  const auto url = std::string(message->uri.buf, message->uri.len);
  std::string query;
  if (message->query.buf != nullptr) {
    query = std::string(message->query.buf, message->query.len);
  }

  Request::headers_type headers;
  const size_t max = std::size(message->headers);
  for (size_t i = 0; i < max && message->headers[i].name.len > 0; i++) {
    auto key = std::string(message->headers[i].name.buf, message->headers[i].name.len);
    const auto value = std::string(message->headers[i].value.buf, message->headers[i].value.len);
    headers[key] = value;
  }

  // Downloading POST data
  ostringstream postData;
  postData.write(message->body.buf, message->body.len);
  const std::string data = postData.str();
  return {ip, is_ssl, method, url, query, headers, data};
}

ServerMongooseImpl::reply ServerMongooseImpl::render(Response &response, const bool is_ssl) {
  // Applied here, on the way out, so every answer carries them: static
  // files, API responses and the error pages a controller returns alike.
  Helpers::add_security_headers(response, is_ssl);
  std::stringstream headers;
  bool has_content_type = false;
  for (const Response::header_type::value_type &v : response.get_headers()) {
    headers << v.first << ": " << v.second << "\r\n";
    if (v.first == "Content-Type") {
      has_content_type = true;
    }
  }
  for (const auto &c : response.get_cookies()) {
    const std::string &name = c.first;
    const std::string &value = c.second.first;
    const Response::cookie_attrs &a = c.second.second;
    if (name.empty() || name.find_first_of("\r\n;= \t") != std::string::npos) {
      continue;
    }
    if (value.find_first_of("\r\n;") != std::string::npos) {
      continue;
    }
    // SameSite=None requires Secure per RFC 6265bis §5.4.7. Browsers will
    // drop a SameSite=None cookie that lacks Secure, so emitting it would
    // silently lose the session. Skip the cookie instead.
    if (boost::algorithm::iequals(a.same_site, "None") && !(a.secure && is_ssl)) {
      continue;
    }
    headers << "Set-Cookie: " << name << "=" << value << "; Path=" << (a.path.empty() ? "/" : a.path);
    if (a.max_age >= 0) {
      headers << "; Max-Age=" << a.max_age;
    }
    if (a.http_only) {
      headers << "; HttpOnly";
    }
    if (a.secure && is_ssl) {
      headers << "; Secure";
    }
    if (!a.same_site.empty()) {
      headers << "; SameSite=" << a.same_site;
    }
    headers << "\r\n";
  }
  // No wildcard Access-Control-Allow-Origin. The bundled SPA is served
  // from the same origin so it does not need CORS, and the wildcard meant
  // any cross-origin page could read responses to authenticated requests
  // that did not require credentials. Operators who need cross-origin
  // access should put a reverse proxy in front and pin Origin there.
  if (response.getCode() == 200 && !has_content_type) {
    headers << "Content-Type: application/json\r\n";
  }
  if (response.getCode() > 299 && !has_content_type) {
    headers << "Content-Type: text/plain\r\n";
  }

  reply out;
  out.code = response.getCode();
  out.headers = headers.str();
  out.body = response.getBody();
  return out;
}

void ServerMongooseImpl::onHttpRequest(mg_connection *connection, mg_http_message *message) {
  bool is_ssl = connection->is_tls;
  auto url = std::string(message->uri.buf, message->uri.len);
  auto method = std::string(message->method.buf, message->method.len);

  // Match the override header on its full name. The comparison used to be
  // bounded by the *incoming* header's length, so strncmp succeeded for any
  // name that is a prefix of it - "X:", "X-H:" and "X-HTTP:" all silently
  // changed the request method, which defeats any upstream proxy or WAF ACL
  // that classifies requests by method. Compare the whole thing, and
  // case-insensitively, since RFC 7230 §3.2 makes field names case-insensitive.
  //
  // A repeated override is ambiguous, so it is ignored rather than resolved:
  // an upstream proxy or WAF that classifies by method has to agree with us on
  // which copy counts, and "first wins" here vs "last wins" there is exactly
  // the disagreement that makes the ACL bypassable. Both backends drop the
  // request's override entirely in that case - see ServerBeastImpl.
  static const std::string kMethodOverride = "X-HTTP-Method-Override";
  const size_t max = std::size(message->headers);
  std::string override_value;
  size_t override_count = 0;
  for (size_t i = 0; i < max && message->headers[i].name.len > 0; i++) {
    const std::string name(message->headers[i].name.buf, message->headers[i].name.len);
    if (message->headers[i].value.len > 0 && boost::algorithm::iequals(name, kMethodOverride)) {
      override_value.assign(message->headers[i].value.buf, message->headers[i].value.len);
      override_count++;
    }
  }
  if (override_count == 1) {
    method = override_value;
  } else if (override_count > 1) {
    logger_->log_error("Ignoring " + std::to_string(override_count) + " conflicting X-HTTP-Method-Override headers");
  }

  for (Controller *ctrl : controllers) {
    if (ctrl->handles(method, url)) {
      char buf[100];
      mg_snprintf(buf, sizeof(buf), "%M", mg_print_ip, &connection->rem);
      auto ip = std::string(buf);
      Request request = build_request(ip, message, is_ssl, method);

      if (use_workers_) {
        // Answered from a worker; mongoose keeps the connection marked as
        // answering until deliver() replies, holding back anything pipelined
        // behind this request.
        job queued;
        queued.connection_id = connection->id;
        queued.controller = ctrl;
        queued.request.reset(new Request(std::move(request)));
        queued.is_ssl = is_ssl;
        const mg_str *connection_header = mg_http_get_header(message, "Connection");
        queued.close = connection_header != nullptr && mg_strcasecmp(*connection_header, mg_str("close")) == 0;
        {
          const std::lock_guard<std::mutex> lock(jobs_mutex_);
          waiting_.insert(connection->id);
          jobs_.push_back(std::move(queued));
        }
        jobs_cv_.notify_one();
        return;
      }
      const std::unique_ptr<Response> response(ctrl->handleRequest(request));
      const reply answer = render(*response, is_ssl);
      mg_http_reply(connection, answer.code, answer.headers.c_str(), "%s", answer.body.c_str());

      return;
    }
  }
  // Same headers as every other answer: this path never builds a Response, so
  // it assembles them from the shared list itself. A framed 404 is still a
  // framed page.
  std::ostringstream not_found_headers;
  not_found_headers << "Content-Type: text/plain\r\n";
  for (const auto &header : Helpers::security_headers(is_ssl)) {
    not_found_headers << header.first << ": " << header.second << "\r\n";
  }
  mg_http_reply(connection, HTTP_NOT_FOUND, not_found_headers.str().c_str(), "Document not found");
}

}  // namespace Mongoose
