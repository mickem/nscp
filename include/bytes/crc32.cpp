// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <bytes/crc32.h>

#include <array>

namespace {
// Built at compile time: the table used to be filled lazily on first use,
// which raced when two NRPE/NSCA io threads computed their first CRC at once
// (one could see the flag set before the table stores and use zeroed entries).
constexpr std::array<unsigned long, 256> make_crc32_table() {
  std::array<unsigned long, 256> table{};
  for (unsigned long i = 0; i < 256; i++) {
    unsigned long crc = i;
    for (int j = 8; j > 0; j--) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320UL;
      else
        crc >>= 1;
    }
    table[i] = crc;
  }
  return table;
}
constexpr std::array<unsigned long, 256> crc32_table = make_crc32_table();
}  // namespace

// Kept for source compatibility: the table is a compile-time constant now.
void generate_crc32_table() {}

unsigned long calculate_crc32(const char *buffer, const std::size_t buffer_size) {
  return calculate_crc32(reinterpret_cast<const unsigned char *>(buffer), buffer_size);
}

unsigned long calculate_crc32(const unsigned char *buffer, const std::size_t buffer_size) {
  unsigned long crc = 0xFFFFFFFF;

  for (std::size_t current_index = 0; current_index < buffer_size; current_index++) {
    const int this_char = buffer[current_index];
    crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
  }

  return (crc ^ 0xFFFFFFFF);
}
