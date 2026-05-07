#include "Response.h"

#include <algorithm>
#include <sstream>
#include <utility>

using namespace std;

namespace {
// Strip CR / LF / NUL from header values. Without this guard a controller
// that splices a request-controlled string (e.g. the Host header) into a
// response header value can be tricked into HTTP response splitting:
//
//   Host: example.com\r\nSet-Cookie: x=y
//
// would turn one Link header into Link plus a forged Set-Cookie. We strip
// rather than reject so a buggy caller does not bring down the response;
// the resulting header value just loses the offending bytes.
string sanitize_header_value(string v) {
  v.erase(std::remove_if(v.begin(), v.end(), [](char c) { return c == '\r' || c == '\n' || c == '\0'; }), v.end());
  return v;
}
string sanitize_header_key(string k) {
  k.erase(std::remove_if(k.begin(), k.end(), [](char c) { return c == '\r' || c == '\n' || c == '\0' || c == ':' || c == ' '; }), k.end());
  return k;
}
}  // namespace

namespace Mongoose {
Response::Response() : code(HTTP_OK), reason(REASON_OK) {}

void Response::setHeader(const string key, string value) { headers[sanitize_header_key(key)] = sanitize_header_value(std::move(value)); }

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
