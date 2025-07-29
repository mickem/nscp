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

 public:
  Request(const std::string ip, bool is_ssl, std::string method, std::string url, std::string query, headers_type headers, std::string data);

  /**
   * Sends a given response to the client
   *
   * @param Response a response for this request
   */
  void writeResponse(Response *response);

  /**
   * Check if the variable given by key is present in GET or POST data
   *
   * @param string the name of the variable
   *
   * @return bool true if the param is present, false else
   */
  bool hasVariable(std::string key);

  /**
   * Get the value for a certain variable
   *
   * @param string the name of the variable
   * @param string the fallback value if the variable doesn't exists
   *
   * @return string the value of the variable if it exists, fallback else
   */
  std::string get(std::string key, std::string fallback = "");
  bool get_bool(std::string key, bool fallback = false);
  long long get_number(std::string key, long long fallback = 0);

  /**
   * Try to get the cookie value
   *
   * @param string the name of the cookie
   * @param string the fallback value
   *
   * @retun the value of the cookie if it exists, fallback else
   */
  std::string getCookie(std::string key, std::string fallback = "");

  /**
   * Handle uploads to the target directory
   *
   * @param string the target directory
   * @param path the posted file path
   */
  void handleUploads();

  std::string getUrl();
  std::string getMethod();
  std::string getData();
  std::string getRemoteIp();

  arg_vector getVariablesVector();
  std::string readHeader(const std::string key);

  std::string get_host();
  bool is_ssl() { return is_ssl_; }

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
