// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Test-only helper: lays down a zip archive so a test can build the ".zip
// module" it needs instead of checking in a binary fixture per variation.

#include <boost/filesystem/path.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace zip_test {

// --- a minimal zip writer ---------------------------------------------------
//
// STORED (uncompressed) entries only, which is all a reader needs to accept.
// The repo has a zip *reader* (bytes::unzip, libzip or miniz depending on the
// platform) but no writer, and pulling one of those backends into a test just
// to lay down a few hundred bytes is more coupling than the job needs.

inline uint32_t crc32_of(const std::string &data) {
  uint32_t crc = 0xFFFFFFFFu;
  for (const unsigned char c : data) {
    crc ^= c;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc ^ 0xFFFFFFFFu;
}

inline void put16(std::string &out, const uint16_t v) {
  out.push_back(static_cast<char>(v & 0xFF));
  out.push_back(static_cast<char>((v >> 8) & 0xFF));
}
inline void put32(std::string &out, const uint32_t v) {
  out.push_back(static_cast<char>(v & 0xFF));
  out.push_back(static_cast<char>((v >> 8) & 0xFF));
  out.push_back(static_cast<char>((v >> 16) & 0xFF));
  out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

struct zip_entry {
  std::string name;
  std::string data;
};

inline void write_zip(const boost::filesystem::path &path, const std::vector<zip_entry> &entries) {
  std::string body;    // local headers + data
  std::string central; // central directory
  for (const zip_entry &e : entries) {
    const uint32_t crc = crc32_of(e.data);
    const uint32_t offset = static_cast<uint32_t>(body.size());

    put32(body, 0x04034b50);                                 // local file header
    put16(body, 20);                                         // version needed
    put16(body, 0);                                          // flags
    put16(body, 0);                                          // method: stored
    put16(body, 0);                                          // mod time
    put16(body, 0);                                          // mod date
    put32(body, crc);
    put32(body, static_cast<uint32_t>(e.data.size()));       // compressed size
    put32(body, static_cast<uint32_t>(e.data.size()));       // uncompressed size
    put16(body, static_cast<uint16_t>(e.name.size()));
    put16(body, 0);                                          // extra length
    body += e.name;
    body += e.data;

    put32(central, 0x02014b50);                              // central directory header
    put16(central, 20);                                      // version made by
    put16(central, 20);                                      // version needed
    put16(central, 0);
    put16(central, 0);
    put16(central, 0);
    put16(central, 0);
    put32(central, crc);
    put32(central, static_cast<uint32_t>(e.data.size()));
    put32(central, static_cast<uint32_t>(e.data.size()));
    put16(central, static_cast<uint16_t>(e.name.size()));
    put16(central, 0);                                       // extra
    put16(central, 0);                                       // comment
    put16(central, 0);                                       // disk number
    put16(central, 0);                                       // internal attrs
    put32(central, 0);                                       // external attrs
    put32(central, offset);
    central += e.name;
  }

  std::string out = body + central;
  put32(out, 0x06054b50);                                    // end of central directory
  put16(out, 0);                                             // this disk
  put16(out, 0);                                             // disk with cd
  put16(out, static_cast<uint16_t>(entries.size()));         // entries on this disk
  put16(out, static_cast<uint16_t>(entries.size()));         // entries total
  put32(out, static_cast<uint32_t>(central.size()));
  put32(out, static_cast<uint32_t>(body.size()));
  put16(out, 0);                                             // comment length

  std::ofstream f(path.string().c_str(), std::ios::binary);
  f.write(out.data(), static_cast<std::streamsize>(out.size()));
}

}  // namespace zip_test
