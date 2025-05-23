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

#include <utils.h>

static unsigned long crc32_table[256];
static bool hascrc32 = false;
void generate_crc32_table() {
  unsigned long poly = 0xEDB88320L;
  for (int i = 0; i < 256; i++) {
    unsigned long crc = i;
    for (int j = 8; j > 0; j--) {
      if (crc & 1)
        crc = (crc >> 1) ^ poly;
      else
        crc >>= 1;
    }
    crc32_table[i] = crc;
  }
  hascrc32 = true;
}
unsigned long calculate_crc32(const char *buffer, const std::size_t buffer_size) {
  if (!hascrc32) generate_crc32_table();

  unsigned long crc = 0xFFFFFFFF;

  for (int current_index = 0; current_index < buffer_size; current_index++) {
    const int this_char = static_cast<unsigned char>(buffer[current_index]);
    crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
  }

  return (crc ^ 0xFFFFFFFF);
}

unsigned long calculate_crc32(const unsigned char *buffer, const std::size_t buffer_size) {
  if (!hascrc32) generate_crc32_table();

  unsigned long crc = 0xFFFFFFFF;

  for (int current_index = 0; current_index < buffer_size; current_index++) {
    const int this_char = buffer[current_index];
    crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
  }

  return (crc ^ 0xFFFFFFFF);
}