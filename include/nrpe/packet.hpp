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
#include <types.hpp>

#include <boost/asio/buffer.hpp>
#include <str/xtos.hpp>
#include <string>
#include <swap_bytes.hpp>

namespace nrpe {
class data {
 public:
  static const short unknownPacket = 0;
  static const short queryPacket = 1;
  static const short responsePacket = 2;
  static const short moreResponsePacket = 3;
  static const short version2 = 2;
  static const short version3 = 3;
  static const short version4 = 3;

  static const std::size_t buffer_offset_v2 = 10;
  static const std::size_t buffer_offset_v3 = 16;

  struct packet_header {
    int16_t packet_version;
    int16_t packet_type;
  };

  struct packet_v2 {
    int16_t packet_version;
    int16_t packet_type;
    uint32_t crc32_value;
    int16_t result_code;
  };

  struct packet_v3 {
    int16_t packet_version;
    int16_t packet_type;
    int32_t crc32_value;
    int16_t result_code;
    int16_t alignment;
    int32_t buffer_length;
    char buffer[1];
  };
};

class length {
  typedef std::size_t size_type;
  static size_type payload_length_;

 public:
  static void set_payload_length(size_type length) { payload_length_ = length; }
  static size_type get_packet_length_v2() { return get_packet_length_v2(payload_length_); }
  static size_type get_min_header_length() { return sizeof(nrpe::data::packet_header); }
  static size_type get_packet_length_v2(size_type payload_length) { return sizeof(nrpe::data::packet_v2) + payload_length * sizeof(char); }
  static size_type get_packet_length_v3(size_type payload_length) { return sizeof(nrpe::data::packet_v3) + payload_length * sizeof(char) - 1; }
  static size_type get_packet_length_v4(size_type payload_length) { return sizeof(nrpe::data::packet_v3) + payload_length * sizeof(char) - 4; }
  static size_type get_payload_length() { return payload_length_; }
  static size_type get_payload_length(size_type packet_length) { return (packet_length - sizeof(nrpe::data::packet_v2)) / sizeof(char); }
};

class nrpe_exception : public std::exception {
  std::string error_;

 public:
  nrpe_exception(std::string error) : error_(error) {}
  ~nrpe_exception() throw() {}
  const char* what() const throw() { return error_.c_str(); }
};

class packet /*: public boost::noncopyable*/ {
 public:
  typedef std::pair<const char*, std::size_t> buffer_holder;

 private:
  char* tmpBuffer;
  std::size_t payload_length_;
  short type_;
  short version_;
  int16_t result_;
  std::string payload_;
  unsigned int crc32_;
  unsigned int calculatedCRC32_;

 public:
  packet(const unsigned int payload_length)
      : tmpBuffer(NULL), payload_length_(payload_length), type_(0), version_(0), result_(0), crc32_(0), calculatedCRC32_(0) {};
  packet(std::vector<char> buffer, unsigned int payload_length) : tmpBuffer(NULL), payload_length_(payload_length) {
    char* tmp = new char[buffer.size() + 1];
    copy(buffer.begin(), buffer.end(), tmp);
    try {
      readFrom(tmp, buffer.size());
    } catch (const nrpe::nrpe_exception& e) {
      delete[] tmp;
      throw e;
    }
    delete[] tmp;
  };
  packet(const char* buffer, std::size_t buffer_length) : tmpBuffer(NULL), payload_length_(length::get_payload_length(buffer_length)) {
    readFrom(buffer, buffer_length);
  };
  packet(short type, short version, int16_t result, std::string payLoad, std::size_t payload_length)
      : tmpBuffer(nullptr),
        payload_length_(payload_length),
        type_(type),
        version_(version),
        result_(result),
        payload_(payLoad),
        crc32_(0),
        calculatedCRC32_(0) {}
  packet()
      : tmpBuffer(NULL),
        payload_length_(nrpe::length::get_payload_length()),
        type_(nrpe::data::unknownPacket),
        version_(nrpe::data::version2),
        result_(0),
        crc32_(0),
        calculatedCRC32_(0) {}
  packet(const packet& other) : tmpBuffer(NULL) {
    payload_ = other.payload_;
    type_ = other.type_;
    version_ = other.version_;
    result_ = other.result_;
    crc32_ = other.crc32_;
    calculatedCRC32_ = other.calculatedCRC32_;
    payload_length_ = other.payload_length_;
  }
  packet& operator=(packet const& other) {
    tmpBuffer = NULL;
    payload_ = other.payload_;
    type_ = other.type_;
    version_ = other.version_;
    result_ = other.result_;
    crc32_ = other.crc32_;
    calculatedCRC32_ = other.calculatedCRC32_;
    payload_length_ = other.payload_length_;
    return *this;
  }

  static packet unknown_response(std::string message) { return packet(nrpe::data::responsePacket, nrpe::data::version2, 3, message, 0); }

  ~packet() { delete[] tmpBuffer; }
  static packet make_request(std::string payload, unsigned int buffer_length, short version) {
    if (version != 2 && version != 4) {
      throw nrpe::nrpe_exception("Invalid NRPE version: " + str::xtos(version) + ", expected 2 or 4");
    }
    return packet(nrpe::data::queryPacket, version, -1, payload, buffer_length);
  }
  static char* payload_offset(nrpe::data::packet_v2* p) { return &reinterpret_cast<char*>(p)[nrpe::data::buffer_offset_v2]; }
  static const char* payload_offset(const nrpe::data::packet_v2* p) { return &reinterpret_cast<const char*>(p)[nrpe::data::buffer_offset_v2]; }
  static const char* payload_offset(const nrpe::data::packet_v3* p) { return &reinterpret_cast<const char*>(p)[nrpe::data::buffer_offset_v3]; }
  static void update_payload(nrpe::data::packet_v2* p, const std::string& payload) {
    char* data = payload_offset(p);
    strncpy(data, payload.c_str(), payload.length());
    data[payload.length()] = 0;
  }
  static void update_payload(nrpe::data::packet_v3* p, const std::string& payload) {
    char* data = &p->buffer[0];
    strncpy(data, payload.c_str(), payload.length());
    data[payload.length()] = 0;
  }
  static std::string fetch_payload(const nrpe::data::packet_v2* p) {
    const char* data = payload_offset(p);
    return std::string(data);
  }
  static std::string fetch_payload(const nrpe::data::packet_v3* p) {
    const char* data = payload_offset(p);
    return std::string(data);
  }

  buffer_holder create_buffer() {
    if (version_ == nrpe::data::version2) {
      return create_buffer_v2();
    }
    return create_buffer_v3();
  }

  buffer_holder create_buffer_v2() {
    delete[] tmpBuffer;
    std::size_t packet_length = nrpe::length::get_packet_length_v2(payload_length_);
    tmpBuffer = new char[packet_length + 1];
    memset(tmpBuffer, 0, packet_length + 1);
    nrpe::data::packet_v2* p = reinterpret_cast<nrpe::data::packet_v2*>(tmpBuffer);
    p->result_code = swap_bytes::hton<int16_t>(result_);
    p->packet_type = swap_bytes::hton<int16_t>(type_);
    p->packet_version = swap_bytes::hton<int16_t>(version_);
    if (payload_.length() >= payload_length_) {
      throw nrpe::nrpe_exception("To much data cant create return packet (truncate data)");
    }
    update_payload(p, payload_);
    p->crc32_value = 0;
    crc32_ = p->crc32_value = swap_bytes::hton<uint32_t>(calculate_crc32(tmpBuffer, static_cast<int>(packet_length)));
    return std::make_pair(tmpBuffer, packet_length);
  }
  buffer_holder create_buffer_v3() {
    delete[] tmpBuffer;
    std::size_t len = payload_.length();
    std::size_t packet_length = version_ == nrpe::data::version3 ? nrpe::length::get_packet_length_v3(len) : nrpe::length::get_packet_length_v4(len);
    tmpBuffer = new char[packet_length + 1];
    memset(tmpBuffer, 0, packet_length + 1);
    nrpe::data::packet_v3* p = reinterpret_cast<nrpe::data::packet_v3*>(tmpBuffer);
    p->result_code = swap_bytes::hton<int16_t>(result_);
    p->packet_type = swap_bytes::hton<int16_t>(type_);
    p->packet_version = swap_bytes::hton<int16_t>(version_);
    p->alignment = 0;
    p->buffer_length = swap_bytes::hton<int32_t>(static_cast<int32_t>(len));
    update_payload(p, payload_);
    p->crc32_value = 0;
    crc32_ = p->crc32_value = swap_bytes::hton<uint32_t>(calculate_crc32(tmpBuffer, static_cast<int>(packet_length)));
    return std::make_pair(tmpBuffer, packet_length);
  }

  std::vector<char> get_buffer() {
    buffer_holder buffer = create_buffer();
    std::vector<char> buf(buffer.first, buffer.first + buffer.second);
    return buf;
  }

  void readFrom(const char* buffer, std::size_t length) {
    if (buffer == NULL) {
      throw nrpe::nrpe_exception("No buffer.");
    }
    if (length < nrpe::length::get_min_header_length()) {
      throw nrpe::nrpe_exception("Packet to short to determine version: " + str::xtos(length) + " < " + str::xtos(nrpe::length::get_min_header_length()));
    }
    const nrpe::data::packet_header* p = reinterpret_cast<const nrpe::data::packet_header*>(buffer);
    int version = swap_bytes::ntoh<int16_t>(p->packet_version);
    if (version == 3 || version == 4) {
      readFromV3(buffer, length);
    } else {
      readFromV2(buffer, length);
    }
  }
  void readFromV2(const char* buffer, std::size_t length) {
    if (length != get_packet_length_v2()) {
      throw nrpe_exception("Invalid packet length: " + str::xtos(length) + " != " + str::xtos(get_packet_length_v2()) +
                           " configured payload is: " + str::xtos(get_payload_length()));
    }
    const nrpe::data::packet_v2* p = reinterpret_cast<const nrpe::data::packet_v2*>(buffer);
    type_ = swap_bytes::ntoh<int16_t>(p->packet_type);
    if (type_ != nrpe::data::queryPacket && type_ != nrpe::data::responsePacket && type_ != nrpe::data::moreResponsePacket) {
      throw nrpe::nrpe_exception("Invalid packet type: " + str::xtos(type_));
    }
    version_ = swap_bytes::ntoh<int16_t>(p->packet_version);
    if (version_ != nrpe::data::version2) {
      throw nrpe::nrpe_exception("Invalid packet version: " + str::xtos(version_));
    }
    crc32_ = swap_bytes::ntoh<uint32_t>(p->crc32_value);
    // Verify CRC32
    // @todo Fix this, currently we need a const buffer so we cannot change the CRC to 0.
    char* tb = new char[length + 1];
    memcpy(tb, buffer, length);
    nrpe::data::packet_v2* p2 = reinterpret_cast<nrpe::data::packet_v2*>(tb);
    p2->crc32_value = 0;
    calculatedCRC32_ = calculate_crc32(tb, static_cast<int>(get_packet_length_v2()));
    delete[] tb;
    if (crc32_ != calculatedCRC32_) {
      throw nrpe::nrpe_exception("Invalid checksum in NRPE packet: " + str::xtos(crc32_) + "!=" + str::xtos(calculatedCRC32_));
    }
    // Verify CRC32 end
    result_ = swap_bytes::ntoh<int16_t>(p->result_code);
    payload_ = fetch_payload(p);
  }

  void readFromV3(const char* buffer, std::size_t length) {
    if (length < nrpe::length::get_packet_length_v3(0)) {
      throw nrpe::nrpe_exception("Invalid packet length: " + str::xtos(length) + " < " + str::xtos(nrpe::length::get_packet_length_v3(0)));
    }

    const nrpe::data::packet_v3* p = reinterpret_cast<const nrpe::data::packet_v3*>(buffer);
    type_ = swap_bytes::ntoh<int16_t>(p->packet_type);
    if (type_ != nrpe::data::queryPacket && type_ != nrpe::data::responsePacket && type_ != nrpe::data::moreResponsePacket) {
      throw nrpe::nrpe_exception("Invalid packet type: " + str::xtos(type_));
    }
    version_ = swap_bytes::ntoh<int16_t>(p->packet_version);
    if (version_ != 3 && version_ != 4) {
      throw nrpe::nrpe_exception("Invalid packet version: " + str::xtos(version_));
    }
    std::size_t payload_length = swap_bytes::ntoh<int32_t>(p->buffer_length);
    if (payload_length < 0 || payload_length > 1024 * 1024) {
      throw nrpe::nrpe_exception("Invalid packet length specified: " + str::xtos(payload_length));
    }
    std::size_t source_data_length = version_ == 4 ? nrpe::length::get_packet_length_v4(payload_length) : nrpe::length::get_packet_length_v3(payload_length);
    if (length < source_data_length) {
      throw nrpe::nrpe_exception("Invalid packet length: " + str::xtos(length) + " != " + str::xtos(source_data_length));
    }

    crc32_ = swap_bytes::ntoh<uint32_t>(p->crc32_value);
    // Verify CRC32
    // @todo Fix this, currently we need a const buffer so we cannot change the CRC to 0.
    char* tb = new char[length + 1];
    memcpy(tb, buffer, length);
    nrpe::data::packet_v3* p2 = reinterpret_cast<nrpe::data::packet_v3*>(tb);
    p2->crc32_value = 0;
    p2->alignment = 0;
    calculatedCRC32_ = calculate_crc32(tb, static_cast<int>(source_data_length));
    delete[] tb;
    if (crc32_ != calculatedCRC32_) {
      throw nrpe::nrpe_exception("Invalid checksum in NRPE packet: " + str::xtos(crc32_) + "!=" + str::xtos(calculatedCRC32_));
    }
    // Verify CRC32 end
    result_ = swap_bytes::ntoh<int16_t>(p->result_code);
    payload_ = fetch_payload(p);
  }

  unsigned short getVersion() const { return version_; }
  unsigned short getType() const { return type_; }
  unsigned short getResult() const { return result_; }
  std::string getPayload() const { return payload_; }
  bool verifyCRC() { return calculatedCRC32_ == crc32_; }
  std::size_t get_packet_length_v2() const { return nrpe::length::get_packet_length_v2(payload_length_); }
  std::size_t get_payload_length() const { return payload_length_; }

  boost::asio::const_buffer to_buffers() {
    std::pair<const char*, std::size_t> tmp_buffer = create_buffer();
    return boost::asio::buffer(tmp_buffer.first, tmp_buffer.second);
  }
  std::string to_string() {
    std::stringstream ss;
    ss << "type: " << type_;
    ss << ", version: " << version_;
    ss << ", result: " << result_;
    ss << ", crc32: " << crc32_;
    ss << ", payload: " << payload_;
    return ss.str();
  }
  static nrpe::packet create_response(unsigned short version, int ret, std::string string, std::size_t buffer_length) {
    return packet(nrpe::data::responsePacket, version, static_cast<int16_t>(ret), string, buffer_length);
  }
  static nrpe::packet create_more_response(int ret, std::string string, std::size_t buffer_length) {
    return packet(nrpe::data::moreResponsePacket, nrpe::data::version2, static_cast<int16_t>(ret), string, buffer_length);
  }
};
}  // namespace nrpe
