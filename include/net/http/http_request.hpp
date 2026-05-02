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

#include <map>
#include <ostream>
#include <sstream>
#include <str/xtos.hpp>
#include <string>
#include <utility>
#include <vector>

namespace http {

inline std::string charToHex(const char c) {
  std::string result;

  char first = (c & 0xF0) / 16;
  first += first > 9 ? 'A' - 10 : '0';
  char second = c & 0x0F;
  second += second > 9 ? 'A' - 10 : '0';

  result.append(1, first);
  result.append(1, second);

  return result;
}

inline std::string uri_encode(const std::string &src) {
  std::string result;

  for (const char iter : src) {
    switch (iter) {
      case ' ':
        result.append(1, '+');
        break;
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '!': case '~': case '*': case '\'': case '(': case ')':
        result.append(1, iter);
        break;
      default:
        result.append(1, '%');
        result.append(charToHex(iter));
        break;
    }
  }

  return result;
}

// An outbound HTTP/1.0 request. Header keys are stored with the casing the
// caller provides so the wire form is predictable for tools and tests; HTTP
// is case-insensitive, but inspection tools and assertions are not.
struct request {
  typedef std::map<std::string, std::string> header_type;
  typedef std::map<std::string, std::string> post_map_type;

  header_type headers_;
  std::string verb_;
  std::string server_;
  std::string path_;
  std::string payload_;

  request() = default;
  request(std::string verb, std::string server, std::string path, std::string payload)
      : verb_(std::move(verb)), server_(std::move(server)), path_(std::move(path)), payload_(std::move(payload)) {
    add_default_headers();
  }
  request(std::string verb, std::string server, std::string path) : verb_(std::move(verb)), server_(std::move(server)), path_(std::move(path)) {
    add_default_headers();
  }

  void set_path(std::string verb, std::string path) {
    verb_ = std::move(verb);
    path_ = std::move(path);
  }
  void add_header(std::string key, std::string value) { headers_[std::move(key)] = std::move(value); }
  void add_default_headers() {
    add_header("Accept", "*/*");
    add_header("Connection", "close");
  }
  void set_payload(std::string data) { payload_ = std::move(data); }

  std::string to_string() const {
    std::stringstream ss;
    ss << "verb: " << verb_ << ", path: " << path_;
    for (const header_type::value_type &v : headers_) ss << ", " << v.first << ": " << v.second;
    return ss.str();
  }

  std::string get_header() const {
    std::stringstream ss;
    const char *crlf = "\r\n";
    ss << verb_ << " " << path_ << " HTTP/1.0" << crlf;
    if (!server_.empty()) {
      ss << "Host: " << server_ << crlf;
    }
    for (const header_type::value_type &v : headers_) ss << v.first << ": " << v.second << crlf;
    ss << crlf;
    return ss.str();
  }
  std::string get_payload() const {
    std::stringstream ss;
    if (!payload_.empty()) ss << payload_;
    return ss.str();
  }

  std::vector<char> get_packet() const {
    std::vector<char> ret;
    const std::string h = get_header();
    ret.insert(ret.end(), h.begin(), h.end());
    const std::string p = get_payload();
    ret.insert(ret.end(), p.begin(), p.end());
    return ret;
  }

  void build_request(std::ostream &os) const {
    const char *crlf = "\r\n";
    os << verb_ << " " << path_ << " HTTP/1.0" << crlf;
    if (!server_.empty()) {
      os << "Host: " << server_ << crlf;
    }
    for (const header_type::value_type &e : headers_) {
      os << e.first << ": " << e.second << crlf;
    }
    os << crlf;
    if (!payload_.empty()) os << payload_;
    os << crlf;
    os << crlf;
  }

  void add_post_payload(const post_map_type &payload_map) {
    std::string data;
    for (const post_map_type::value_type &v : payload_map) {
      if (!data.empty()) data += "&";
      data += uri_encode(v.first);
      data += "=";
      data += uri_encode(v.second);
    }
    add_header("Content-Length", str::xtos(data.size()));
    add_header("Content-Type", "application/x-www-form-urlencoded");
    verb_ = "POST";
    payload_ = data;
  }

  void add_post_payload(const std::string &content_type, const std::string &payload_data) {
    add_header("Content-Length", str::xtos(payload_data.size()));
    add_header("Content-Type", content_type);
    verb_ = "POST";
    payload_ = payload_data;
  }
};

}  // namespace http
