/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils.h>

static unsigned long crc32_table[256];
static bool hascrc32 = false;
void generate_crc32_table(void) {
	unsigned long crc, poly;
	int i, j;
	poly = 0xEDB88320L;
	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;
		}
		crc32_table[i] = crc;
	}
	hascrc32 = true;
}
unsigned long calculate_crc32(const char *buffer, int buffer_size) {
	if (!hascrc32)
		generate_crc32_table();
	register unsigned long crc;
	int this_char;
	int current_index;

	crc = 0xFFFFFFFF;

	for (current_index = 0; current_index < buffer_size; current_index++) {
		this_char = (int)buffer[current_index];
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
	}

	return (crc ^ 0xFFFFFFFF);
}

unsigned long calculate_crc32(const unsigned char *buffer, int buffer_size) {
	if (!hascrc32)
		generate_crc32_table();
	register unsigned long crc;
	int this_char;
	int current_index;

	crc = 0xFFFFFFFF;

	for (current_index = 0; current_index < buffer_size; current_index++) {
		this_char = (int)buffer[current_index];
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32_table[(crc ^ this_char) & 0xFF];
	}

	return (crc ^ 0xFFFFFFFF);
}