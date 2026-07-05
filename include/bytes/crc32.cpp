// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <bytes/crc32.h>

static unsigned long crc32_table[256];
static bool has_crc32 = false;
void generate_crc32_table() {
  for (int i = 0; i < 256; i++) {
    unsigned long crc = i;
    for (int j = 8; j > 0; j--) {
      constexpr auto poly = 0xEDB88320L;
      if (crc & 1)
        crc = (crc >> 1) ^ poly;
      else
        crc >>= 1;
    }
    crc32_table[i] = crc;
  }
  has_crc32 = true;
}
unsigned long calculate_crc32(const char *buffer, const std::size_t buffer_size) {
  if (!has_crc32) generate_crc32_table();

  unsigned long crc = 0xFFFFFFFF;

  for (std::size_t current_index = 0; current_index < buffer_size; current_index++) {
    const int this_char = static_cast<unsigned char>(buffer[current_index]);
    crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
  }

  return (crc ^ 0xFFFFFFFF);
}

unsigned long calculate_crc32(const unsigned char *buffer, const std::size_t buffer_size) {
  if (!has_crc32) generate_crc32_table();

  unsigned long crc = 0xFFFFFFFF;

  for (std::size_t current_index = 0; current_index < buffer_size; current_index++) {
    const int this_char = buffer[current_index];
    crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
  }

  return (crc ^ 0xFFFFFFFF);
}