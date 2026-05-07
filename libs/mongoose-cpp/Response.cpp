#include "Response.h"

#include <sstream>
#include <utility>

using namespace std;

namespace Mongoose {
Response::Response() : code(HTTP_OK), reason(REASON_OK) {}

void Response::setHeader(const string key, string value) { headers[key] = std::move(value); }

bool Response::hasHeader(const string key) { return headers.find(key) != headers.end(); }

void Response::setCookie(const string key, string value) { setCookie(key, std::move(value), cookie_attrs{}); }

void Response::setCookie(string key, string value, cookie_attrs attrs) { cookies[std::move(key)] = std::make_pair(std::move(value), std::move(attrs)); }

void Response::setCode(const int code_, std::string reason_) {
  code = code_;
  reason = std::move(reason_);
}

void Response::setCodeOk() {
  code = HTTP_OK;
  reason = REASON_OK;
}

std::string Response::getCookie(const std::string key) const {
  const auto cit = cookies.find(key);
  if (cit == cookies.end()) {
    return "";
  }
  return cit->second.first;
}

}  // namespace Mongoose
