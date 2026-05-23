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

// Decode an application/x-www-form-urlencoded fragment: "%XX" -> the byte,
// "+" -> space, everything else passthrough. Invalid / truncated "%XX"
// escapes are copied through verbatim (matches mg_url_decode with
// is_form_url_encoded=1, which only failed on undersized output buffers —
// not reachable here since we always size to the input length).
std::string decode_form(const char *data, std::size_t len) {
  std::string out;
  out.reserve(len);
  const auto hex_val = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
  };
  for (std::size_t i = 0; i < len; ++i) {
    const char c = data[i];
    if (c == '+') {
      out.push_back(' ');
    } else if (c == '%' && i + 2 < len && std::isxdigit(static_cast<unsigned char>(data[i + 1])) && std::isxdigit(static_cast<unsigned char>(data[i + 2]))) {
      out.push_back(static_cast<char>((hex_val(data[i + 1]) << 4) | hex_val(data[i + 2])));
      i += 2;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

inline std::string decode_form(const std::string &s) { return decode_form(s.data(), s.size()); }

// Locate the first form-encoded value matching `key` in `query`.
// Returns true on hit (mirrors mg_http_get_var: case-sensitive key,
// first occurrence wins, returns just the decoded value).
bool readVariable(const std::string &query, const std::string &key, std::string &output) {
  if (query.empty()) return false;
  std::size_t i = 0;
  const std::size_t n = query.size();
  while (i < n) {
    std::size_t pair_end = query.find('&', i);
    if (pair_end == std::string::npos) pair_end = n;
    const std::size_t eq = query.find('=', i);
    if (eq != std::string::npos && eq < pair_end) {
      const std::string k = decode_form(query.data() + i, eq - i);
      if (k == key) {
        output = decode_form(query.data() + eq + 1, pair_end - eq - 1);
        return true;
      }
    }
    i = pair_end + 1;
  }
  return false;
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
  // data is "var1=val1&var2=val2...". Split on '&', decode key and value
  // independently via the same form-decode used by readVariable().
  while (getline(f, s, '&')) {
    const auto eq_pos = s.find('=');
    string key, val;
    if (eq_pos != string::npos) {
      key = decode_form(s.data(), eq_pos);
      val = decode_form(s.data() + eq_pos + 1, s.size() - eq_pos - 1);
    } else {
      key = decode_form(s);
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
  if (readVariable(query, key, output)) {
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
