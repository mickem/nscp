// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/asio/buffer.hpp>
#include <str/xtos.hpp>
#include <string>

namespace check_nt {
class check_nt_exception {
  std::wstring error_;

 public:
  explicit check_nt_exception(const std::wstring& error) : error_(error) {}
  std::wstring getMessage() { return error_; }
};
class check_nt_packet_exception : public check_nt_exception {
 public:
  explicit check_nt_packet_exception(const std::wstring& error) : check_nt_exception(error) {}
};

class packet /*: public boost::noncopyable*/ {
 private:
  std::string data_;

 public:
  packet() {};
  packet(std::vector<char> buffer) { data_ = std::string(buffer.begin(), buffer.end()); };
  packet(std::string data) : data_(data) {}
  packet(const packet& other) : data_(other.data_) {}
  packet& operator=(packet const& other) {
    data_ = other.data_;
    return *this;
  }

  ~packet() {
    // delete [] tmpBuffer;
  }

  std::vector<char> get_buffer() const { return std::vector<char>(data_.begin(), data_.end()); }
  std::string get_payload() const { return data_; }

  std::size_t get_packet_length() const { return data_.length(); }
  boost::asio::const_buffer to_buffers() const { return boost::asio::buffer(get_buffer(), get_packet_length()); }
  std::string to_string() const {
    std::stringstream ss;
    ss << "data: " << data_;
    return ss.str();
  }
};
}  // namespace check_nt