#include "Client.hpp"

#include <boost/make_shared.hpp>
#include <boost/thread/thread.hpp>
#include <string>
#include <utility>

#include "StreamResponse.h"
#include "string_response.hpp"

// clang-format off
// Has to be after boost or we get namespace clashes
#include <mongoose.h>
// clang-format on

using namespace std;
using namespace Mongoose;

class Handler {
 public:
  std::string payload_;
  std::string error;
  boost::shared_ptr<Response> response;

  Handler(std::string payload) : payload_(std::move(payload)) {}

  void send(struct mg_connection *c, const std::string &data) { mg_send(c, data.c_str(), data.length()); }

  static void ev_handler(struct mg_connection *c, const int ev, void *ev_data) {
    auto *handler = static_cast<Handler *>(c->fn_data);
    handler->handler(c, ev, ev_data);
  }
  void handler(struct mg_connection *c, int ev, void *ev_data) {
    auto *hm = static_cast<struct mg_http_message *>(ev_data);

    switch (ev) {
      case MG_EV_CONNECT:
        mg_send(c, payload_.c_str(), payload_.length());
        break;
      case MG_EV_ERROR:
        error = std::string("connect() failed: ") + strerror(*(int *)ev_data);
        break;
      case MG_EV_HTTP_MSG:
        parseReply(hm);
        break;
      default:
        break;
    }
  }

  void parseReply(struct mg_http_message *hm) {
    size_t i = 0;
    auto message = std::string(hm->message.buf, hm->message.len);
    response = boost::make_shared<mcp::string_response>(mg_http_status(hm), message);
    for (i = 0; hm->headers[i].name.len > 0; i++) {
      response->setHeader(std::string(hm->headers[i].name.buf, hm->headers[i].name.len), std::string(hm->headers[i].value.buf, hm->headers[i].value.len));
    }
  }
};
namespace Mongoose {
Client::Client(std::string url) : url_(std::move(url)) {}

Client::~Client() = default;

boost::shared_ptr<Response> Client::fetch(std::string verb, Client::header_type hdr, std::string payload) const {
  mg_mgr mgr{};

  mg_mgr_init(&mgr);
  std::stringstream request;

  std::string uri = mg_url_uri(url_.c_str());

  request << verb + " " + uri + " HTTP/1.0\r\n";
  for (const header_type::value_type &v : hdr) {
    request << v.first << ": " << v.second << "\r\n";
  }
  request << "\n\r";
  Handler handler(request.str());

  mg_http_connect(&mgr, url_.c_str(), &Handler::ev_handler, &handler);
  // while (!handler.is_done()) {
  mg_mgr_poll(&mgr, 1000);
  //}
  mg_mgr_free(&mgr);
  return handler.response;
}

}  // namespace Mongoose
