#include "Request.h"

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/thread.hpp>
#include <cctype>
#include <cstring>
#include <sstream>
#include <str/xtos.hpp>
#include <string>
#include <utility>
#include <vector>

// clang-format off
// Has to be after boost or we get namespace clashes
#include "mongoose_wrapper.h"
// clang-format on

using namespace std;

namespace {

// Case-insensitive substring search; returns nullptr if not found.
const char *mg_strcasestr(const char *big_str, const char *small_str) {
  const std::size_t big_len = std::strlen(big_str);
  const std::size_t small_len = std::strlen(small_str);
  if (small_len == 0) return big_str;
  if (big_len < small_len) return nullptr;

  const auto ieq = [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); };
  const auto *first = big_str;
  const auto *last = big_str + big_len;
  const auto it = std::search(first, last, small_str, small_str + small_len, ieq);
  return (it == last) ? nullptr : it;
}

// Extracts the value of `var_name` from a Cookie header into `dst`.
// Returns the decoded length, or:
//   -1 if not found / invalid input
//   -2 if dst is null or dst_size == 0
//   -3 if dst is too small to hold the value
long long mg_get_cookie(const char *cookie_header, const char *var_name, char *dst, size_t dst_size) {
  if (dst == nullptr || dst_size == 0) return -2;
  if (var_name == nullptr || cookie_header == nullptr) {
    dst[0] = '\0';
    return -1;
  }

  const std::size_t name_len = std::strlen(var_name);
  const char *s = cookie_header;
  const char *const end = s + std::strlen(s);
  dst[0] = '\0';

  for (; (s = mg_strcasestr(s, var_name)) != nullptr; s += name_len) {
    if (s[name_len] != '=') continue;
    s += name_len + 1;
    const char *p = std::strchr(s, ' ');
    if (p == nullptr) p = end;
    if (p > s && p[-1] == ';') --p;
    if (*s == '"' && p > s + 1 && p[-1] == '"') {
      ++s;
      --p;
    }
    const auto len = static_cast<size_t>(p - s);
    if (len >= dst_size) return -3;
    std::memcpy(dst, s, len);
    dst[len] = '\0';
    return static_cast<long long>(len);
  }
  return -1;
}

bool readVariable(const mg_str data, const string &key, string &output) {
  if (data.len == 0) {
    return false;
  }
  // The decoded variable value can never exceed the length of the source data
  // itself, so sizing the buffer to that length guarantees a single call.
  std::string buffer(data.len + 1, '\0');
  const int ret = mg_http_get_var(&data, key.c_str(), &buffer[0], static_cast<int>(buffer.size()));

  if (ret <= 0) {
    return false;
  }

  buffer.resize(static_cast<size_t>(ret));
  output = std::move(buffer);
  return true;
}

}  // namespace

namespace Mongoose {

Request::Request(std::string ip, bool is_ssl, std::string method, std::string url, std::string query, headers_type headers, std::string data)
    : is_ssl_(is_ssl),
      method(std::move(method)),
      url(std::move(url)),
      query(std::move(query)),
      data(std::move(data)),
      ip(std::move(ip)),
      headers(std::move(headers)) {}

bool Request::hasVariable(const string &key) const { return headers.find(key) != headers.end(); }

Request::arg_vector get_var_vector(const char *data, size_t data_len) {
  Request::arg_vector ret;

  if (data == nullptr || data_len == 0) return ret;

  istringstream f(string(data, data_len));
  string s;
  // RAII buffer for url-decoded fragments.
  std::vector<char> tmp(data_len + 1);
  // data is "var1=val1&var2=val2...". Find variable first
  while (getline(f, s, '&')) {
    const auto eq_pos = s.find('=');
    string key, val;
    if (eq_pos != string::npos) {
      key = s.substr(0, eq_pos);
      val = s.substr(eq_pos + 1);
    } else {
      key = s;
    }
    if (mg_url_decode(key.c_str(), static_cast<int>(key.length()), tmp.data(), static_cast<int>(tmp.size()), 1) == -1) {
      return ret;
    }
    key = tmp.data();
    if (!val.empty()) {
      if (mg_url_decode(val.c_str(), static_cast<int>(val.length()), tmp.data(), static_cast<int>(tmp.size()), 1) == -1) {
        return ret;
      }
      val = tmp.data();
    }
    ret.emplace_back(std::move(key), std::move(val));
  }
  return ret;
}

Request::arg_vector Request::getVariablesVector() const { return get_var_vector(query.c_str(), query.size()); }

std::string Request::readHeader(const std::string &key) const {
  const auto it = headers.find(key);
  return (it != headers.end()) ? it->second : std::string();
}

std::string Request::get_host() const {
  if (hasVariable("Host")) {
    return (is_ssl() ? "https://" : "http://") + readHeader("Host");
  }
  return {};
}

string Request::get(const string &key, string fallback) const {
  string output;
  if (readVariable(mg_str(query.c_str()), key, output)) {
    return output;
  }
  return fallback;
}

bool Request::get_bool(const string &key, const bool fallback) const {
  const std::string v = boost::algorithm::to_lower_copy(get(key, fallback ? "true" : "false"));
  return v == "true";
}

long long Request::get_number(const std::string &key, const long long fallback) const { return str::stox<long long>(get(key, str::xtos(fallback)), fallback); }

string Request::getCookie(const string &key, string fallback) const {
  const auto it = headers.find("cookie");
  if (it == headers.end() || it->second.empty()) {
    return fallback;
  }
  const std::string &header = it->second;
  // The decoded cookie value can never exceed the length of the Cookie header
  // itself, so sizing the buffer to that length guarantees a single call.
  std::string buffer(header.size() + 1, '\0');
  const long long ret = mg_get_cookie(header.c_str(), key.c_str(), &buffer[0], buffer.size());
  if (ret < 0) {
    return fallback;
  }
  buffer.resize(static_cast<size_t>(ret));
  return buffer;
}
}  // namespace Mongoose
