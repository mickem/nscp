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

#include <utils.h>

#include <boost/date_time.hpp>
#include <str/xtos.hpp>
#include <swap_bytes.hpp>
#include <utility>

namespace nsca {

constexpr boost::posix_time::ptime EPOCH_TIME_T(boost::gregorian::date(1970, 1, 1));

class data {
 public:
  static constexpr short transmitted_iuv_size = 128;
  static constexpr int16_t version3 = 3;

  typedef struct data_packet : boost::noncopyable {
    int16_t packet_version;
    uint32_t crc32_value;
    uint32_t timestamp;
    int16_t return_code;
    char data[];

    char* get_data_offset(const unsigned int offset) { return &data[offset]; }
    const char* get_data_offset(const unsigned int offset) const { return &data[offset]; }
    const char* get_host_ptr() const { return get_data_offset(0); }
    const char* get_desc_ptr(const unsigned int host_len) const { return get_data_offset(host_len); }
    const char* get_result_ptr(const unsigned int host_len, const unsigned int desc_len) const { return get_data_offset(host_len + desc_len); }
    char* get_host_ptr() { return get_data_offset(0); }
    char* get_desc_ptr(const unsigned int host_len) { return get_data_offset(host_len); }
    char* get_result_ptr(const unsigned int host_len, const unsigned int desc_len) { return get_data_offset(host_len + desc_len); }
  } data_packet;

  /* initialization packet containing IV and timestamp */
  typedef struct iv_packet {
    char iv[transmitted_iuv_size];
    uint32_t timestamp;
  } init_packet;
};

/* data packet containing service check results */
class nsca_exception : public std::exception {
  std::string msg_;

 public:
  nsca_exception() = default;
  explicit nsca_exception(std::string msg) : msg_(std::move(msg)) {}

  nsca_exception(const nsca_exception& other) noexcept : exception(other) { msg_ = other.msg_; }
  nsca_exception& operator=(const nsca_exception& other) noexcept = default;
  ~nsca_exception() noexcept override = default;

  const char* what() const noexcept override { return msg_.c_str(); }
};

class length {
 public:
  typedef unsigned int size_type;
  static constexpr std::size_t host_length = 64;
  static constexpr std::size_t desc_length = 128;

 public:
  static size_type payload_length_;
  static void set_payload_length(size_type length) { payload_length_ = length; }
  static size_type get_packet_length() { return get_packet_length(payload_length_); }
  static size_type get_packet_length(size_type output_length) {
    return sizeof(data::data_packet) + output_length * sizeof(char) + host_length * sizeof(char) + desc_length * sizeof(char);
  }
  static size_type get_payload_length() { return payload_length_; }
  static size_type get_payload_length(size_type packet_length) {
    return (packet_length - (host_length * sizeof(char) + desc_length * sizeof(char) + sizeof(data::data_packet))) / sizeof(char);
  }
  class iv {
   public:
    static constexpr unsigned int payload_length_ = data::transmitted_iuv_size;
    static size_type get_packet_length() { return sizeof(data::iv_packet); }
    static size_type get_payload_length() { return payload_length_; }
  };
};

class packet {
 public:
  std::string service;
  std::string result;
  std::string host;
  unsigned int code;
  uint32_t time;
  unsigned int payload_length_;

  explicit packet(std::string _host, const unsigned int payload_length = 512, const int time_delta = 0)
      : host(std::move(_host)), code(0), payload_length_(payload_length) {
    const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time() + boost::posix_time::seconds(time_delta);
    const boost::posix_time::time_duration diff = now - EPOCH_TIME_T;
    time = static_cast<uint32_t>(diff.total_seconds());
  }
  explicit packet(const unsigned int payload_length) : code(0), payload_length_(payload_length) {
    const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    const boost::posix_time::time_duration diff = now - EPOCH_TIME_T;
    time = static_cast<uint32_t>(diff.total_seconds());
  }
  packet() : code(0), time(0), payload_length_(length::get_payload_length()) {
    const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    const boost::posix_time::time_duration diff = now - EPOCH_TIME_T;
    time = static_cast<uint32_t>(diff.total_seconds());
  }
  packet& operator=(const packet& other) = default;

  std::string to_string() const {
    return "host: " + host + ", " + "service: " + service + ", " + "code: " + str::xtos(code) + ", " + "time: " + str::xtos(time) + ", " + "result: " + result;
  }

  void parse_data(const char* buffer, std::size_t buffer_len) {
    char* tmp = new char[buffer_len];
    memcpy(tmp, buffer, buffer_len);
    auto* data = reinterpret_cast<data::data_packet*>(tmp);
    time = swap_bytes::ntoh<uint32_t>(data->timestamp);
    code = swap_bytes::ntoh<int16_t>(data->return_code);

    host = data->get_host_ptr();
    service = data->get_desc_ptr(length::host_length);
    result = data->get_result_ptr(length::host_length, length::desc_length);

    const auto crc32 = swap_bytes::ntoh<uint32_t>(data->crc32_value);
    data->crc32_value = 0;
    const unsigned int calculated_crc32 = calculate_crc32(tmp, buffer_len);
    delete[] tmp;
    if (crc32 != calculated_crc32) throw nsca_exception("Invalid crc: " + str::xtos(crc32) + " != " + str::xtos(calculated_crc32));
  }
  void validate_lengths() const {
    if (service.length() >= length::desc_length)
      throw nsca_exception("Description field to long: " + str::xtos(service.length()) + " > " + str::xtos(length::desc_length));
    if (host.length() >= length::host_length) throw nsca_exception("Host field to long: " + str::xtos(host.length()) + " > " + str::xtos(length::host_length));
    if (result.length() >= get_payload_length())
      throw nsca_exception("Result field to long: " + str::xtos(result.length()) + " > " + str::xtos(get_payload_length()));
  }

  static void copy_string(char* data, const std::string& value, std::string::size_type max_length) {
    memset(data, 0, max_length);
    value.copy(data, value.size() > max_length ? max_length : value.size());
  }

  void get_buffer(std::string& buffer, const int servertime = 0) const {
    data::data_packet* data = reinterpret_cast<data::data_packet*>(&*buffer.begin());
    if (buffer.size() < get_packet_length()) throw nsca_exception("Buffer is to short: " + str::xtos(buffer.length()) + " > " + str::xtos(get_packet_length()));

    data->packet_version = swap_bytes::hton<int16_t>(data::version3);
    if (servertime != 0)
      data->timestamp = swap_bytes::hton<uint32_t>(static_cast<uint32_t>(servertime));
    else
      data->timestamp = swap_bytes::hton<uint32_t>(time);
    data->return_code = swap_bytes::hton<int16_t>(static_cast<int16_t>(code));
    data->crc32_value = swap_bytes::hton<uint32_t>(0);

    copy_string(data->get_host_ptr(), host, length::host_length);
    copy_string(data->get_desc_ptr(length::host_length), service, length::desc_length);
    copy_string(data->get_result_ptr(length::host_length, length::desc_length), result, get_payload_length());

    const unsigned int calculated_crc32 = calculate_crc32(buffer.c_str(), static_cast<int>(buffer.size()));
    data->crc32_value = swap_bytes::hton<uint32_t>(calculated_crc32);
  }
  std::string get_buffer() const {
    std::string buffer;
    get_buffer(buffer);
    return buffer;
  }
  unsigned int get_packet_length() const { return length::get_packet_length(payload_length_); }
  unsigned int get_payload_length() const { return payload_length_; }
};

class iv_packet {
  std::string iv;
  uint32_t time;

 public:
  iv_packet(std::string iv, const uint32_t time) : iv(std::move(iv)), time(time) {}
  iv_packet(std::string iv, const boost::posix_time::ptime now) : iv(std::move(iv)), time(ptime_to_unixtime(now)) {}
  explicit iv_packet(const std::string& buffer) : time(0) { parse(buffer); }
  uint32_t ptime_to_unixtime(const boost::posix_time::ptime now) {
    const boost::posix_time::time_duration diff = now - EPOCH_TIME_T;
    return static_cast<uint32_t>(diff.total_seconds());
  }
  uint32_t get_time() const { return time; }
  std::string get_iv() const { return iv; }

  std::string get_buffer() const {
    if (iv.size() != length::iv::get_payload_length())
      throw nsca_exception("Invalid IV size: " + str::xtos(iv.size()) + " != " + str::xtos(length::iv::get_payload_length()));
    data::iv_packet data{};
    memcpy(data.iv, iv.c_str(), iv.size());
    data.timestamp = swap_bytes::hton<uint32_t>(time);
    const char* src = reinterpret_cast<char*>(&data);
    std::string buffer(src, length::iv::get_packet_length());
    return buffer;
  }
  void parse(const std::string& buffer) {
    if (buffer.size() < length::iv::get_packet_length())
      throw nsca_exception("Buffer is to short: " + str::xtos(buffer.length()) + " > " + str::xtos(length::iv::get_packet_length()));
    const auto* data = reinterpret_cast<const data::iv_packet*>(buffer.c_str());
    iv = std::string(data->iv, data::transmitted_iuv_size);
    time = swap_bytes::ntoh<uint32_t>(data->timestamp);
  }
};
}  // namespace nsca
