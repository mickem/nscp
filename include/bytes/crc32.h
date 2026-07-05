// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdio>

void generate_crc32_table();
unsigned long calculate_crc32(const char *buffer, std::size_t buffer_size);
unsigned long calculate_crc32(const unsigned char *buffer, std::size_t buffer_size);
