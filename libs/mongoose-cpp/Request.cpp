#include "Request.h"

#include <str/xtos.hpp>

#include "ext/mongoose.h"

#include <boost/thread.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <string>

using namespace std;

static int lowercase(const char *s) { return tolower(*(const unsigned char *)s); }

static int mg_strncasecmp(const char *s1, const char *s2, size_t len) {
  int diff = 0;

  if (len > 0) do {
      diff = lowercase(s1++) - lowercase(s2++);
    } while (diff == 0 && s1[-1] != '\0' && --len > 0);

  return diff;
}

static void mg_strlcpy(register char *dst, register const char *src, size_t n) {
  for (; *src != '\0' && n > 1; n--) {
    *dst++ = *src++;
  }
  *dst = '\0';
}

static const char *mg_strcasestr(const char *big_str, const char *small_str) {
  std::size_t i, big_len = strlen(big_str), small_len = strlen(small_str);

  for (i = 0; i <= big_len - small_len; i++) {
    if (mg_strncasecmp(big_str + i, small_str, small_len) == 0) {
      return big_str + i;
    }
  }

  return NULL;
}

static long long mg_get_cookie(const char *cookie_header, const char *var_name, char *dst, size_t dst_size) {
  const char *s, *p, *end;
  std::size_t name_len;
  long long len = -1;

  if (dst == NULL || dst_size == 0) {
    len = -2;
  } else if (var_name == NULL || (s = cookie_header) == NULL) {
    len = -1;
    dst[0] = '\0';
  } else {
    name_len = strlen(var_name);
    end = s + strlen(s);
    dst[0] = '\0';

    for (; (s = mg_strcasestr(s, var_name)) != NULL; s += name_len) {
      if (s[name_len] == '=') {
        s += name_len + 1;
        if ((p = strchr(s, ' ')) == NULL) p = end;
        if (p[-1] == ';') p--;
        if (*s == '"' && p[-1] == '"' && p > s + 1) {
          s++;
          p--;
        }
        if ((size_t)(p - s) < dst_size) {
          len = p - s;
          mg_strlcpy(dst, s, (size_t)len + 1);
        } else {
          len = -3;
        }
        break;
      }
    }
  }
  return len;
}

namespace Mongoose {

Request::Request(const std::string ip, bool is_ssl, std::string method, std::string url, std::string query, headers_type headers, std::string data)
    : is_ssl_(is_ssl), method(method), ip(ip), url(url), query(query), headers(headers), data(data) {}

string Request::getUrl() { return url; }

string Request::getMethod() { return method; }

string Request::getData() { return data; }

string Request::getRemoteIp() { return ip; }

bool Request::hasVariable(string key) { return headers.find(key) != headers.end(); }

Request::arg_vector get_var_vector(const char *data, size_t data_len) {
  Request::arg_vector ret;

  if (data == NULL || data_len == 0) return ret;

  istringstream f(string(data, data_len));
  string s;
  char *tmp = new char[data_len + 1];
  // data is "var1=val1&var2=val2...". Find variable first
  while (getline(f, s, '&')) {
    string::size_type eq_pos = s.find('=');
    string key, val;
    if (eq_pos != string::npos) {
      key = s.substr(0, eq_pos);
      val = s.substr(eq_pos + 1);
    } else {
      key = s;
    }
    if (mg_url_decode(key.c_str(), static_cast<int>(key.length()), tmp, static_cast<int>(data_len + 1), 1) == -1) {
      delete[] tmp;
      return ret;
    }
    key = tmp;
    if (val.length() > 0) {
      if (mg_url_decode(val.c_str(), static_cast<int>(val.length()), tmp, static_cast<int>(data_len + 1), 1) == -1) {
        delete[] tmp;
        return ret;
      }
      val = tmp;
    }
    ret.push_back(Request::arg_entry(key, val));
  }
  delete[] tmp;
  return ret;
}

Request::arg_vector Request::getVariablesVector() { return get_var_vector(query.c_str(), query.size()); }

std::string Request::readHeader(const std::string key) { return headers[key]; }

std::string Request::get_host() {
  if (hasVariable("Host")) {
    std::string proto = is_ssl() ? "https://" : "http://";
    return proto + readHeader("Host");
  }
  return "";
}

bool readVariable(const struct mg_str data, string key, string &output) {
  int size = 1024, ret;
  char *buffer = new char[size];

  do {
    ret = mg_http_get_var(&data, key.c_str(), buffer, size);

    if (ret == -1 || ret == 0) {
      delete[] buffer;
      return false;
    }

    if (ret == -2) {
      size *= 2;
      delete[] buffer;
      buffer = new char[size];
    }
  } while (ret == -2);

  output = string(buffer);
  delete[] buffer;

  return true;
}

string Request::get(string key, string fallback) {
  string output;
  // Looking on the query string
  struct mg_str dataField;
  dataField.ptr = query.c_str();
  dataField.len = query.size();
  if (readVariable(dataField, key, output)) {
    return output;
  }

  // Looking on the POST data
  //         dataField = data.c_str();
  //         if (dataField != NULL && readVariable(dataField, key, output)) {
  //             return output;
  //         }

  return fallback;
}

bool Request::get_bool(string key, bool fallback) {
  std::string v = boost::algorithm::to_lower_copy(get(key, fallback ? "true" : "false"));
  return v == "true";
}
long long Request::get_number(std::string key, long long fallback) { return str::stox<long long>(get("page", str::xtos(fallback)), fallback); }

string Request::getCookie(string key, string fallback) {
  long long ret = -1;
  int size = 1024;
  char *buffer = new char[size];
  do {
    ret = mg_get_cookie(headers["cookie"].c_str(), key.c_str(), buffer, size);
    if (ret >= 0) {
      std::string tmp = buffer;
      delete[] buffer;
      return tmp;
    }
    if (ret == -1LL) {
      delete[] buffer;
      return fallback;
    }
    if (ret == -3LL) {
      size *= 2;
      delete[] buffer;
      buffer = new char[size];
    }
  } while (ret == -3LL);
  delete[] buffer;
  return fallback;
}
}  // namespace Mongoose
