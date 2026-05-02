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

#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <str/xtos.hpp>
#include <string>
#include <utility>
#include <vector>

namespace http {

inline bool find_line_end(const char c1, const char c2) { return c1 == '\r' && c2 == '\n'; }
inline bool find_header_break(const char c1, const char c2) { return c1 == '\n' && c2 == '\r'; }

// An inbound HTTP response. Header keys are normalised to lower case at
// storage time so callers can do `headers_.find("transfer-encoding")` without
// caring about server casing (HTTP/1.1 headers are case-insensitive per
// RFC 7230). Values have surrounding whitespace stripped because the line
// parser sees `Header: value\r` and we don't want callers to deal with that.
struct response {
  typedef std::map<std::string, std::string> header_type;

  header_type headers_;
  std::string http_version_;
  unsigned int status_code_;
  std::string status_message_;
  std::string payload_;

  response() : status_code_(0) {}
  response(std::string http_version, const unsigned int status_code, std::string status_message)
      : http_version_(std::move(http_version)), status_code_(status_code), status_message_(std::move(status_message)) {}

  // Parse a raw response off the wire (status line + headers + body).
  // Returns an empty response when the buffer doesn't contain a complete
  // status line; the remainder is best-effort.
  explicit response(const std::vector<char> &data) : status_code_(0) {
    auto its = data.begin();
    const auto ite = std::adjacent_find(its, data.end(), find_line_end);
    if (ite == data.end()) return;
    parse_status_line(std::string(its, ite));
    its = ite + 2;
    while (true) {
      const auto iterator = std::adjacent_find(its, data.end(), find_line_end);
      if (iterator == data.end()) break;
      const std::string line(its, iterator);
      if (line.empty()) {
        payload_ = std::string(iterator + 2, data.end());
        break;
      }
      add_header(line);
      its = iterator + 2;
    }
  }

  void parse_status_line(const std::string &line) {
    const std::string::size_type pos = line.find(' ');
    if (pos == std::string::npos) {
      http_version_ = line;
      status_code_ = 500;
    } else {
      http_version_ = line.substr(0, pos);
      status_code_ = str::stox<int>(line.substr(pos + 1));
    }
  }

  void add_header(std::string key, std::string value) {
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    if (first == std::string::npos)
      value.clear();
    else
      value = value.substr(first, last - first + 1);
    headers_[std::move(key)] = std::move(value);
  }
  void add_header(const std::string &line) {
    const std::string::size_type pos = line.find(':');
    if (pos == std::string::npos)
      add_header(line, "");
    else
      add_header(line.substr(0, pos), line.substr(pos + 1));
  }

  bool is_2xx() const { return status_code_ >= 200 && status_code_ < 300; }
  std::string get_payload() const { return payload_; }

  static response create_timeout(std::string message) {
    response r;
    r.status_code_ = 99;
    r.payload_ = std::move(message);
    return r;
  }
};

}  // namespace http
