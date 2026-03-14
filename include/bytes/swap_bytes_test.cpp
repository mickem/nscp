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

#include <gtest/gtest.h>

#include <bytes/swap_bytes.hpp>

#include <cstdint>

// --- SwapBytes direct tests ---

TEST(swap_bytes, swap_uint16) {
  uint16_t val = 0x0102;
  uint16_t swapped = swap_bytes::SwapBytes<uint16_t, 2>(val);
  EXPECT_EQ(swapped, 0x0201);
}

TEST(swap_bytes, swap_uint32) {
  uint32_t val = 0x01020304;
  uint32_t swapped = swap_bytes::SwapBytes<uint32_t, 4>(val);
  EXPECT_EQ(swapped, 0x04030201u);
}

TEST(swap_bytes, swap_uint64) {
  uint64_t val = 0x0102030405060708ULL;
  uint64_t swapped = swap_bytes::SwapBytes<uint64_t, 8>(val);
  EXPECT_EQ(swapped, 0x0807060504030201ULL);
}

TEST(swap_bytes, swap_is_own_inverse_16) {
  uint16_t val = 0xABCD;
  uint16_t double_swapped = swap_bytes::SwapBytes<uint16_t, 2>(swap_bytes::SwapBytes<uint16_t, 2>(val));
  EXPECT_EQ(double_swapped, val);
}

TEST(swap_bytes, swap_is_own_inverse_32) {
  uint32_t val = 0xDEADBEEF;
  uint32_t double_swapped = swap_bytes::SwapBytes<uint32_t, 4>(swap_bytes::SwapBytes<uint32_t, 4>(val));
  EXPECT_EQ(double_swapped, val);
}

TEST(swap_bytes, swap_is_own_inverse_64) {
  uint64_t val = 0xCAFEBABEDEADC0DEULL;
  uint64_t double_swapped = swap_bytes::SwapBytes<uint64_t, 8>(swap_bytes::SwapBytes<uint64_t, 8>(val));
  EXPECT_EQ(double_swapped, val);
}

// --- EndianSwapBytes same-endian is identity ---

TEST(swap_bytes, endian_swap_same_endian_is_identity) {
  uint32_t val = 0x12345678;
  uint32_t result = swap_bytes::EndianSwapBytes<BIG_ENDIAN_ORDER, BIG_ENDIAN_ORDER, uint32_t>(val);
  EXPECT_EQ(result, val);

  result = swap_bytes::EndianSwapBytes<LITTLE_ENDIAN_ORDER, LITTLE_ENDIAN_ORDER, uint32_t>(val);
  EXPECT_EQ(result, val);
}

TEST(swap_bytes, endian_swap_different_endian_swaps) {
  uint16_t val = 0x0102;
  uint16_t result = swap_bytes::EndianSwapBytes<BIG_ENDIAN_ORDER, LITTLE_ENDIAN_ORDER, uint16_t>(val);
  EXPECT_EQ(result, 0x0201);
}

// --- ntoh / hton round-trip ---

TEST(swap_bytes, ntoh_hton_roundtrip_16) {
  uint16_t val = 0x1234;
  EXPECT_EQ(swap_bytes::ntoh(swap_bytes::hton(val)), val);
}

TEST(swap_bytes, ntoh_hton_roundtrip_32) {
  uint32_t val = 0xDEADBEEF;
  EXPECT_EQ(swap_bytes::ntoh(swap_bytes::hton(val)), val);
}

TEST(swap_bytes, ntoh_hton_roundtrip_64) {
  uint64_t val = 0x0102030405060708ULL;
  EXPECT_EQ(swap_bytes::ntoh(swap_bytes::hton(val)), val);
}

// --- ltoh / htol round-trip ---

TEST(swap_bytes, ltoh_htol_roundtrip_16) {
  uint16_t val = 0x5678;
  EXPECT_EQ(swap_bytes::ltoh(swap_bytes::htol(val)), val);
}

TEST(swap_bytes, ltoh_htol_roundtrip_32) {
  uint32_t val = 0xCAFEBABE;
  EXPECT_EQ(swap_bytes::ltoh(swap_bytes::htol(val)), val);
}

TEST(swap_bytes, ltoh_htol_roundtrip_64) {
  uint64_t val = 0xFEDCBA9876543210ULL;
  EXPECT_EQ(swap_bytes::ltoh(swap_bytes::htol(val)), val);
}

// --- On little-endian host, ntoh should swap; on big-endian, it should not ---

TEST(swap_bytes, ntoh_known_value_16) {
  // Network byte order (big-endian) 0x0102 -> host order
  uint16_t network = 0x0102;
  uint16_t host = swap_bytes::ntoh(network);

#if defined(BOOST_ENDIAN_LITTLE_BYTE)
  EXPECT_EQ(host, 0x0201);
#else
  EXPECT_EQ(host, 0x0102);
#endif
}

TEST(swap_bytes, hton_known_value_32) {
  uint32_t host = 0x01020304;
  uint32_t network = swap_bytes::hton(host);

#if defined(BOOST_ENDIAN_LITTLE_BYTE)
  EXPECT_EQ(network, 0x04030201u);
#else
  EXPECT_EQ(network, 0x01020304u);
#endif
}

// --- ltoh on little-endian host should be identity ---

TEST(swap_bytes, ltoh_on_host) {
  uint32_t val = 0xAABBCCDD;
  uint32_t result = swap_bytes::ltoh(val);

#if defined(BOOST_ENDIAN_LITTLE_BYTE)
  EXPECT_EQ(result, val);
#else
  EXPECT_EQ(result, 0xDDCCBBAAu);
#endif
}

// --- int16_t (signed) ---

TEST(swap_bytes, signed_int16_swap) {
  int16_t val = 0x0102;
  int16_t swapped = swap_bytes::SwapBytes<int16_t, 2>(val);
  EXPECT_EQ(static_cast<uint16_t>(swapped), 0x0201);
}

