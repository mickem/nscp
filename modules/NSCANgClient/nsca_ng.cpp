// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nsca_ng.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace nsca_ng {

std::string escape_field(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (const char c : value) {
    if (c == '\\') {
      out += "\\\\";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == ';') {
      // B1: ';' is the Nagios external-command field separator; any unescaped
      // semicolon inside a field would corrupt parsing on the server side.
      out += "\\;";
    } else {
      out += c;
    }
  }
  return out;
}

std::string build_check_result_command(const std::string &host, const std::string &service, int code, const std::string &output, long timestamp) {
  std::ostringstream oss;
  oss << "[" << timestamp << "] ";
  if (service.empty()) {
    oss << "PROCESS_HOST_CHECK_RESULT;" << escape_field(host) << ";" << code << ";" << escape_field(output);
  } else {
    oss << "PROCESS_SERVICE_CHECK_RESULT;" << escape_field(host) << ";" << escape_field(service) << ";" << code << ";" << escape_field(output);
  }
  return oss.str();
}

std::string build_moin_request(const std::string &session_id) { return "MOIN 1 " + session_id; }

std::string build_push_request(const std::size_t length) { return "PUSH " + std::to_string(length); }

server_response parse_server_response(const std::string &line) {
  server_response resp;
  resp.kind = server_response::type::unknown;

  if (line.empty()) return resp;

  const auto first_space = line.find(' ');
  const std::string keyword_raw = (first_space == std::string::npos) ? line : line.substr(0, first_space);
  const std::string rest = (first_space == std::string::npos) ? std::string() : line.substr(first_space + 1);

  std::string keyword = keyword_raw;
  std::transform(keyword.begin(), keyword.end(), keyword.begin(), [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });

  if (keyword == "OKAY") {
    resp.kind = server_response::type::okay;
    resp.message = rest;
  } else if (keyword == "FAIL") {
    resp.kind = server_response::type::fail;
    resp.message = rest;
  } else if (keyword == "BAIL") {
    resp.kind = server_response::type::bail;
    resp.message = rest;
  } else if (keyword == "MOIN") {
    resp.kind = server_response::type::moin;
    resp.message = rest;
  }
  return resp;
}

}  // namespace nsca_ng
