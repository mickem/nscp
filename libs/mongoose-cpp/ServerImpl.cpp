#include "ServerImpl.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <fstream>
#include <nsclient/nsclient_exception.hpp>
#include <optional>
#include <sstream>
#include <string>

using namespace std;
using namespace Mongoose;

std::string load_file(const std::string &path, const std::string &hint) {
  try {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  } catch (const std::exception &e) {
    throw nsclient::nsclient_exception("Failed to load " + hint + " from " + path + ": " + e.what());
  }
}

std::pair<std::string, std::string> load_certificates(const std::string &cert_path, const std::string &key_path) {
  auto cert = load_file(cert_path, "certificate");
  if (key_path.empty()) {
    return {cert, cert};
  }
  auto key = load_file(key_path, "private key");
  return {cert, key};
}

boost::posix_time::ptime now() { return boost::get_system_time(); }

std::string tmp_log;
bool logged_cert_issue = false;
void log_wrapper(char c, void *ptr) {
  if (ptr == nullptr) {
    return;
  }
  if (c == '\n' || c == '\r') {
    if (tmp_log.length() == 0) {
      return;
    }
    WebLogger *logger = reinterpret_cast<WebLogger *>(ptr);
    if (boost::algorithm::contains(tmp_log, "alert certificate unknown")) {
      if (!logged_cert_issue) {
        logger->log_error("This could be due to self-signed certificates: " + tmp_log);
        logged_cert_issue = true;
      }
    } else if (boost::algorithm::contains(tmp_log, ":error:")) {
      logger->log_error(tmp_log);
    } else {
      logger->log_info(tmp_log);
    }

    tmp_log = "";
  } else {
    tmp_log += c;
  }
}

namespace Mongoose {
ServerImpl::ServerImpl(WebLoggerPtr logger) : stop_thread_(false), logger_(logger) {
  mg_log_set_fn(&log_wrapper, logger_.get());
  mg_log_set(MG_LL_ERROR);
  memset(&mgr, 0, sizeof(struct mg_mgr));
  mg_mgr_init(&mgr);
}

ServerImpl::~ServerImpl() {
  stop();
  mg_log_set_fn(&log_wrapper, nullptr);

  vector<Controller *>::iterator it;
  for (it = controllers.begin(); it != controllers.end(); it++) {
    delete (*it);
  }
  controllers.clear();
}

void ServerImpl::setSsl(std::string &new_certificate, std::string &new_key) {
#if MG_ENABLE_OPENSSL
  try {
    auto cert_and_key = load_certificates(new_certificate, new_key);
    certificate = cert_and_key.first;
    key = cert_and_key.second;
  } catch (const nsclient::nsclient_exception &e) {
    logger_->log_error("Failed to load certificates: " + e.reason());
  }
#else
  logger_->log_error("Not compiled with TLS");
#endif
}

void ServerImpl::thread_proc() {
  try {
    while (true) {
      mg_mgr_poll(&mgr, 1000);
      if (stop_thread_) {
        mg_mgr_free(&mgr);
        return;
      }
    }
  } catch (...) {
    logger_->log_error("Mongoose error");
  }
}

void ServerImpl::start(std::string bind) {
  mg_http_listen(&mgr, bind.c_str(), ServerImpl::event_handler, (void *)this);
  thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&ServerImpl::thread_proc, this)));
}

void ServerImpl::stop() {
  if (thread_) {
    stop_thread_ = true;
    thread_->interrupt();
    thread_->join();
  }
  thread_.reset();
}

void ServerImpl::registerController(Controller *controller) { controllers.push_back(controller); }

void ServerImpl::event_handler(struct mg_connection *connection, int ev, void *ev_data) {
  if (connection->fn_data != NULL) {
    ServerImpl *impl = (ServerImpl *)connection->fn_data;
    if (ev == MG_EV_ACCEPT) {
#if MG_ENABLE_OPENSSL
      impl->initTls(connection);
#else
      logger_->log_error("Not compiled with TLS support");
#endif
    }
    if (ev == MG_EV_HTTP_MSG) {
      struct mg_http_message *message = (struct mg_http_message *)ev_data;
      impl->onHttpRequest(connection, message);
    }
  }
}
#if MG_ENABLE_OPENSSL
void ServerImpl::initTls(struct mg_connection *connection) {
  if (certificate.empty()) {
    return;
  }
  struct mg_tls_opts opts{};
  memset(&opts, 0, sizeof(struct mg_tls_opts));
  // TODO: Should we add name?
  opts.cert = mg_str(certificate.c_str());
  opts.key = mg_str(key.c_str());
  mg_tls_init(connection, &opts);
}
#endif

Request build_request(const std::string ip, struct mg_http_message *message, bool is_ssl, const std::string method) {
  std::string url = std::string(message->uri.buf, message->uri.len);
  std::string query;
  if (message->query.buf != NULL) {
    query = std::string(message->query.buf, message->query.len);
  }

  Request::headers_type headers;
  size_t max = sizeof(message->headers) / sizeof(message->headers[0]);
  for (size_t i = 0; i < max && message->headers[i].name.len > 0; i++) {
    std::string key = std::string(message->headers[i].name.buf, message->headers[i].name.len);
    std::string value = std::string(message->headers[i].value.buf, message->headers[i].value.len);
    headers[key] = value;
  }

  // Downloading POST data
  ostringstream postData;
  postData.write(message->body.buf, message->body.len);
  std::string data = postData.str();
  return Request(ip, is_ssl, method, url, query, headers, data);
}

void ServerImpl::onHttpRequest(struct mg_connection *connection, struct mg_http_message *message) {
  bool is_ssl = connection->is_tls;
  std::string url = std::string(message->uri.buf, message->uri.len);
  std::string method = std::string(message->method.buf, message->method.len);

  size_t max = sizeof(message->headers) / sizeof(message->headers[0]);
  for (size_t i = 0; i < max && message->headers[i].name.len > 0; i++) {
    struct mg_str *k = &message->headers[i].name, *v = &message->headers[i].value;
    if (message->headers[i].value.len > 0 && strncmp(message->headers[i].name.buf, "X-HTTP-Method-Override", message->headers[i].name.len) == 0) {
      method = std::string(message->headers[i].value.buf, message->headers[i].value.len);
    }
  }

  for (Controller *ctrl : controllers) {
    if (ctrl->handles(method, url)) {
      char buf[100];
      mg_snprintf(buf, sizeof(buf), "%M", mg_print_ip, &connection->rem);
      std::string ip = std::string(buf);
      Request request = build_request(ip, message, is_ssl, method);

      Response *response = ctrl->handleRequest(request);
      std::stringstream headers;
      bool has_content_type = false;
      for (const Response::header_type::value_type &v : response->get_headers()) {
        headers << v.first << ": " << v.second << "\r\n";
        if (v.first == "Content-Type") {
          has_content_type = true;
        }
      }
      headers << "Access-Control-Allow-Origin: *\r\n";
      if (response->getCode() == 200 && !has_content_type) {
        headers << "Content-Type: application/json\r\n";
      }
      if (response->getCode() > 299 && !has_content_type) {
        headers << "Content-Type: text/plain\r\n";
      }

      mg_http_reply(connection, response->getCode(), headers.str().c_str(), "%s", response->getBody().c_str());

      return;
    }
  }
  mg_http_reply(connection, HTTP_NOT_FOUND, "", "Document not found");
}

}  // namespace Mongoose
