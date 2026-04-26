#pragma once

#include "Response.h"
#include "dll_defines.hpp"
#ifdef WIN32
#pragma warning(disable : 4251)
#endif

#include <map>
#include <string>
#include <vector>

/**
 * Request is a wrapper for the clients requests
 */
namespace Mongoose {
class NSCAPI_EXPORT Request {
 public:
  typedef std::pair<std::string, std::string> arg_entry;
  typedef std::vector<arg_entry> arg_vector;
  typedef std::map<std::string, std::string> headers_type;

  Request(std::string ip, bool is_ssl, std::string method, std::string url, std::string query, headers_type headers, std::string data);

  /**
   * Sends a given response to the client
   *
   * @param response a response for this request
   */
  void writeResponse(Response* response);

  /**
   * Check if the variable given by key is present in GET or POST data
   *
   * @param key the name of the variable
   *
   * @return bool true if the param is present, false else
   */
  bool hasVariable(const std::string& key) const;

  /**
   * Get the value for a certain variable
   *
   * @param key the name of the variable
   * @param fallback the fallback value if the variable doesn't exists
   *
   * @return string the value of the variable if it exists, fallback else
   */
  std::string get(const std::string& key, std::string fallback = "") const;
  bool get_bool(const std::string& key, bool fallback = false) const;
  long long get_number(const std::string& key, long long fallback = 0) const;

  headers_type& get_headers() { return headers; }
  const headers_type& get_headers() const { return headers; }

  /**
   * Try to get the cookie value
   *
   * @param key the name of the cookie
   * @param fallback the fallback value
   *
   * @retun the value of the cookie if it exists, fallback else
   */
  std::string getCookie(const std::string& key, std::string fallback = "") const;

  const std::string& getUrl() const { return url; }
  const std::string& getMethod() const { return method; }
  const std::string& getData() const { return data; }
  const std::string& getRemoteIp() const { return ip; }

  arg_vector getVariablesVector() const;
  std::string readHeader(const std::string& key) const;

  std::string get_host() const;
  bool is_ssl() const { return is_ssl_; }

 private:
  bool is_ssl_;
  std::string method;
  std::string url;
  std::string query;
  std::string data;
  std::string ip;
  headers_type headers;
};
}  // namespace Mongoose
