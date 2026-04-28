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
#include <vector>
#include <str/xtos.hpp>
#include <string>

namespace http {

inline bool find_line_end(const char c1, const char c2) { return c1 == '\r' && c2 == '\n'; }
inline bool find_header_break(const char c1, const char c2) { return c1 == '\n' && c2 == '\r'; }

static std::string charToHex(const char c) {
  std::string result;

  char first = (c & 0xF0) / 16;
  first += first > 9 ? 'A' - 10 : '0';
  char second = c & 0x0F;
  second += second > 9 ? 'A' - 10 : '0';

  result.append(1, first);
  result.append(1, second);

  return result;
}

static std::string uri_encode(const std::string &src) {
  std::string result;

  for (const char iter : src) {
    switch (iter) {
      case ' ':
        result.append(1, '+');
        break;
        // alnum
      case 'A':
      case 'B':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J':
      case 'K':
      case 'L':
      case 'M':
      case 'N':
      case 'O':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
      case 'T':
      case 'U':
      case 'V':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z':
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
      case 'i':
      case 'j':
      case 'k':
      case 'l':
      case 'm':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w':
      case 'x':
      case 'y':
      case 'z':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        // mark
      case '-':
      case '_':
      case '.':
      case '!':
      case '~':
      case '*':
      case '\'':
      case '(':
      case ')':
        result.append(1, iter);
        break;
        // escape
      default:
        result.append(1, '%');
        result.append(charToHex(iter));
        break;
    }
  }

  return result;
}

struct packet {
  typedef std::map<std::string, std::string> header_type;
  typedef std::map<std::string, std::string> post_map_type;

  header_type headers_;
  std::string verb_;
  std::string server_;
  std::string path_;
  std::string payload_;
  int status_code_;

  packet() : status_code_(0) {}
  packet(std::string verb, std::string server, std::string path, std::string payload)
      : verb_(std::move(verb)), server_(std::move(server)), path_(std::move(path)), payload_(std::move(payload)), status_code_(0) {
    add_default_headers();
  }
  packet(std::string verb, std::string server, std::string path) : verb_(std::move(verb)), server_(std::move(server)), path_(std::move(path)), status_code_(0) {
    add_default_headers();
  }
  explicit packet(const std::vector<char> &data) : status_code_(0) {
    auto its = data.begin();
    const auto ite = std::adjacent_find(its, data.end(), find_line_end);
    if (ite == data.end()) return;
    parse_http_response(std::string(its, ite));
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

  void set_path(std::string verb, std::string path) {
    verb_ = std::move(verb);
    path_ = std::move(path);
  }
  void parse_http_response(const std::string &line) {
    const std::string::size_type pos = line.find(' ');
    if (pos == std::string::npos)
      set_http_response(line, "500");
    else
      set_http_response(line.substr(0, pos), line.substr(pos + 1));
  }
  void set_http_response(const std::string &version, const std::string &code) { status_code_ = str::stox<int>(code); }
  void add_header(std::string key, std::string value) { headers_[std::move(key)] = std::move(value); }
  void add_header(const std::string &line) {
    const std::string::size_type pos = line.find(':');
    if (pos == std::string::npos)
      add_header(line, "");
    else
      add_header(line.substr(0, pos), line.substr(pos + 1));
  }
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

  static packet create_timeout(std::string message) {
    packet p;
    p.status_code_ = 99;
    p.payload_ = std::move(message);
    return p;
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
  response(const response &other) = default;
  response &operator=(const response &other) = default;

  void add_header(std::string key, std::string value) { headers_[std::move(key)] = std::move(value); }
  void add_header(const std::string &line) {
    const std::string::size_type pos = line.find(':');
    if (pos == std::string::npos)
      add_header(line, "");
    else
      add_header(line.substr(0, pos), line.substr(pos + 1));
  }

  bool is_2xx() const { return status_code_ >= 200 && status_code_ < 300; }
};

}  // namespace http