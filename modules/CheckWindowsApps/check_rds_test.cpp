// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_rds_internal.hpp"

nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

using namespace check_rds::check_rds_internal;

TEST(CheckRdsLicenses, KeyPackTypeNamesMatchTheDocumentedValues) {
  EXPECT_EQ(keypack_type_name(0), "unknown");
  EXPECT_EQ(keypack_type_name(1), "retail");
  EXPECT_EQ(keypack_type_name(2), "volume");
  EXPECT_EQ(keypack_type_name(3), "concurrent");
  EXPECT_EQ(keypack_type_name(4), "temporary");
  EXPECT_EQ(keypack_type_name(5), "open");
  EXPECT_EQ(keypack_type_name(6), "built-in");
}

TEST(CheckRdsLicenses, UnknownKeyPackTypesRenderTheRawValue) { EXPECT_EQ(keypack_type_name(42), "type_42"); }

TEST(CheckRdsSessionLoad, ServicesIsNotAUserSession) {
  EXPECT_FALSE(is_session_instance("Services"));
  EXPECT_TRUE(is_session_instance("Console"));
  EXPECT_TRUE(is_session_instance("RDP-Tcp 55"));
}

TEST(CheckRdsLicenses, KeyPackDefaultsAreEmpty) {
  const license_key_pack pack;
  EXPECT_EQ(pack.id, 0);
  EXPECT_EQ(pack.total, 0);
  EXPECT_EQ(pack.issued, 0);
  EXPECT_EQ(pack.available, 0);
  EXPECT_TRUE(pack.description.empty());
}
