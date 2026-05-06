/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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

std::string build_push_request(std::size_t length) { return "PUSH " + std::to_string(length); }

server_response parse_server_response(const std::string &line) {
  server_response resp;
  resp.kind = server_response::type::unknown;

  if (line.empty()) return resp;

  const auto first_space = line.find(' ');
  const std::string keyword_raw = (first_space == std::string::npos) ? line : line.substr(0, first_space);
  const std::string rest = (first_space == std::string::npos) ? std::string() : line.substr(first_space + 1);

  std::string keyword = keyword_raw;
  std::transform(keyword.begin(), keyword.end(), keyword.begin(), [](unsigned char c) { return std::toupper(c); });

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
