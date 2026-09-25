// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "kube_quantity.hpp"

#include <gtest/gtest.h>

using kube_checks::parse_quantity;
using kube_checks::quantity_to_bytes;
using kube_checks::quantity_to_millicores;

TEST(KubeQuantity, CpuForms) {
  EXPECT_EQ(quantity_to_millicores("0"), 0);
  EXPECT_EQ(quantity_to_millicores("250m"), 250);
  EXPECT_EQ(quantity_to_millicores("1"), 1000);
  EXPECT_EQ(quantity_to_millicores("4"), 4000);
  EXPECT_EQ(quantity_to_millicores("0.5"), 500);
  EXPECT_EQ(quantity_to_millicores("1.25"), 1250);
  EXPECT_EQ(quantity_to_millicores("100u"), 0) << "microcores round to the nearest millicore";
  EXPECT_EQ(quantity_to_millicores("1500u"), 2);
  EXPECT_EQ(quantity_to_millicores("50000000n"), 50);
}

TEST(KubeQuantity, MemoryForms) {
  EXPECT_EQ(quantity_to_bytes("128974848"), 128974848);
  EXPECT_EQ(quantity_to_bytes("100Mi"), 100LL * 1024 * 1024);
  EXPECT_EQ(quantity_to_bytes("1.5Gi"), 1536LL * 1024 * 1024);
  EXPECT_EQ(quantity_to_bytes("1Ki"), 1024);
  EXPECT_EQ(quantity_to_bytes("2Ti"), 2LL * 1024 * 1024 * 1024 * 1024);
  EXPECT_EQ(quantity_to_bytes("129M"), 129000000);
  EXPECT_EQ(quantity_to_bytes("1G"), 1000000000);
  EXPECT_EQ(quantity_to_bytes("8k"), 8000);
  EXPECT_EQ(quantity_to_bytes("16386764Ki"), 16386764LL * 1024);
}

TEST(KubeQuantity, ExponentForms) {
  EXPECT_EQ(quantity_to_bytes("2e3"), 2000);
  EXPECT_EQ(quantity_to_bytes("129e6"), 129000000);
  EXPECT_EQ(quantity_to_bytes("1.5E3"), 1500) << "an E followed by digits is an exponent";
  EXPECT_EQ(quantity_to_bytes("1E"), 1000000000000000000LL) << "a lone E is exa";
  EXPECT_EQ(quantity_to_bytes("5e-1"), 1) << "rounds to the nearest byte";
}

TEST(KubeQuantity, SignsAndWhitespace) {
  EXPECT_EQ(quantity_to_bytes(" 10Mi "), 10LL * 1024 * 1024);
  EXPECT_EQ(quantity_to_bytes("\"10Mi\""), 10LL * 1024 * 1024) << "a quoted value as it may be copied from JSON";
  EXPECT_EQ(quantity_to_bytes("+3"), 3);
  EXPECT_EQ(quantity_to_bytes("-3"), -3);
}

TEST(KubeQuantity, MalformedInputIsRejected) {
  double v = 0;
  EXPECT_FALSE(parse_quantity("", v));
  EXPECT_FALSE(parse_quantity("abc", v));
  EXPECT_FALSE(parse_quantity("10X", v));
  EXPECT_FALSE(parse_quantity("10MiB", v));
  EXPECT_FALSE(parse_quantity("1.2.3", v));
  EXPECT_FALSE(parse_quantity("m", v));
  EXPECT_FALSE(parse_quantity("2e3Mi", v)) << "an exponent takes no suffix";
  EXPECT_FALSE(parse_quantity("-", v));
  EXPECT_EQ(quantity_to_millicores(""), -1);
  EXPECT_EQ(quantity_to_millicores("lots"), -1);
  EXPECT_EQ(quantity_to_bytes("lots"), -1);
}
