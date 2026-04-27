/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include "check_os_updates.hpp"

#include <gtest/gtest.h>

using os_updates_check::classify_update;
using os_updates_check::os_updates_data;
using os_updates_check::os_updates_obj;
using os_updates_check::update_info;

// ============================================================================
// classify_update tests
// ============================================================================

TEST(CheckOsUpdates, classify_security_category) {
  update_info u;
  classify_update("Security Updates", "Important", u);
  EXPECT_TRUE(u.is_security);
  EXPECT_FALSE(u.is_critical);
  EXPECT_EQ(u.category, "Security Updates");
  EXPECT_EQ(u.severity, "Important");
}

TEST(CheckOsUpdates, classify_critical_category) {
  update_info u;
  classify_update("Critical Updates", "Critical", u);
  EXPECT_FALSE(u.is_security);
  EXPECT_TRUE(u.is_critical);
}

TEST(CheckOsUpdates, classify_critical_severity_only) {
  update_info u;
  classify_update("Updates", "Critical", u);
  EXPECT_FALSE(u.is_security);
  EXPECT_TRUE(u.is_critical);
}

TEST(CheckOsUpdates, classify_neither) {
  update_info u;
  classify_update("Updates", "Low", u);
  EXPECT_FALSE(u.is_security);
  EXPECT_FALSE(u.is_critical);
}

TEST(CheckOsUpdates, classify_case_insensitive) {
  update_info u;
  classify_update("security updates", "moderate", u);
  EXPECT_TRUE(u.is_security);
  EXPECT_FALSE(u.is_critical);
}

TEST(CheckOsUpdates, classify_empty) {
  update_info u;
  classify_update("", "", u);
  EXPECT_FALSE(u.is_security);
  EXPECT_FALSE(u.is_critical);
  EXPECT_EQ(u.category, "");
  EXPECT_EQ(u.severity, "");
}

// ============================================================================
// update_info getter tests
// ============================================================================

TEST(CheckOsUpdates, update_info_defaults) {
  update_info u;
  EXPECT_FALSE(u.is_security);
  EXPECT_FALSE(u.is_critical);
  EXPECT_FALSE(u.reboot_required);
  EXPECT_EQ(u.get_is_security(), "false");
  EXPECT_EQ(u.get_is_critical(), "false");
  EXPECT_EQ(u.get_reboot_required(), "false");
}

TEST(CheckOsUpdates, update_info_getters) {
  update_info u;
  u.title = "KB5031234";
  u.severity = "Important";
  u.category = "Security Updates";
  u.is_security = true;
  u.reboot_required = true;
  EXPECT_EQ(u.get_title(), "KB5031234");
  EXPECT_EQ(u.get_severity(), "Important");
  EXPECT_EQ(u.get_category(), "Security Updates");
  EXPECT_EQ(u.get_is_security(), "true");
  EXPECT_EQ(u.get_reboot_required(), "true");
}

// ============================================================================
// os_updates_obj tests
// ============================================================================

TEST(CheckOsUpdates, obj_defaults) {
  os_updates_obj o;
  EXPECT_EQ(o.count, 0);
  EXPECT_EQ(o.security, 0);
  EXPECT_EQ(o.critical, 0);
  EXPECT_EQ(o.important, 0);
  EXPECT_EQ(o.reboot_required, 0);
  EXPECT_FALSE(o.fetch_succeeded);
  EXPECT_EQ(o.get_status(), "pending");
  EXPECT_EQ(o.show(), "update status pending");
}

TEST(CheckOsUpdates, obj_show_error_state) {
  os_updates_obj o;
  o.fetch_succeeded = false;
  o.error = "WUA failed";
  EXPECT_EQ(o.get_status(), "error");
  EXPECT_EQ(o.show(), "update query failed: WUA failed");
}

TEST(CheckOsUpdates, obj_show_no_updates) {
  os_updates_obj o;
  o.fetch_succeeded = true;
  EXPECT_EQ(o.get_status(), "ok");
  EXPECT_EQ(o.show(), "no updates available");
}

TEST(CheckOsUpdates, obj_show_with_security) {
  os_updates_obj o;
  o.fetch_succeeded = true;
  o.count = 5;
  o.security = 2;
  o.critical = 0;
  EXPECT_EQ(o.get_status(), "critical");
  EXPECT_EQ(o.show(), "5 updates available (2 security)");
}

TEST(CheckOsUpdates, obj_show_with_critical_and_security) {
  os_updates_obj o;
  o.fetch_succeeded = true;
  o.count = 7;
  o.security = 3;
  o.critical = 2;
  EXPECT_EQ(o.get_status(), "critical");
  EXPECT_EQ(o.show(), "7 updates available (2 critical, 3 security)");
}

TEST(CheckOsUpdates, obj_show_only_general) {
  os_updates_obj o;
  o.fetch_succeeded = true;
  o.count = 4;
  o.security = 0;
  o.critical = 0;
  EXPECT_EQ(o.get_status(), "warning");
  EXPECT_EQ(o.show(), "4 updates available");
}

TEST(CheckOsUpdates, obj_recompute) {
  os_updates_obj o;

  update_info a;
  a.is_security = true;
  a.severity = "Important";
  o.updates.push_back(a);

  update_info b;
  b.is_critical = true;
  b.severity = "Critical";
  b.reboot_required = true;
  o.updates.push_back(b);

  update_info c;
  c.severity = "Important";
  c.is_security = true;
  c.reboot_required = true;
  o.updates.push_back(c);

  update_info d;
  d.severity = "Low";
  o.updates.push_back(d);

  o.recompute();
  EXPECT_EQ(o.count, 4);
  EXPECT_EQ(o.security, 2);
  EXPECT_EQ(o.critical, 1);
  EXPECT_EQ(o.important, 2);
  EXPECT_EQ(o.reboot_required, 2);
}

TEST(CheckOsUpdates, obj_get_titles) {
  os_updates_obj o;
  update_info a;
  a.title = "KB1";
  update_info b;
  b.title = "KB2";
  o.updates.push_back(a);
  o.updates.push_back(b);
  EXPECT_EQ(o.get_titles(), "KB1; KB2");
}

TEST(CheckOsUpdates, obj_get_titles_empty) {
  os_updates_obj o;
  EXPECT_EQ(o.get_titles(), "");
}

// ============================================================================
// os_updates_data TTL behavior
// ============================================================================

TEST(CheckOsUpdates, data_default_ttl_one_hour) {
  os_updates_data d;
  EXPECT_EQ(d.get_ttl_seconds(), 3600);
}

TEST(CheckOsUpdates, data_set_ttl) {
  os_updates_data d;
  d.set_ttl_seconds(60);
  EXPECT_EQ(d.get_ttl_seconds(), 60);
}

TEST(CheckOsUpdates, data_get_returns_default_before_fetch) {
  os_updates_data d;
  os_updates_obj snapshot = d.get();
  EXPECT_FALSE(snapshot.fetch_succeeded);
  EXPECT_EQ(snapshot.count, 0);
}

// fetch() will exercise the WUA COM API; we don't run it here because it
// requires admin / network access and may take 30+ seconds. The data class is
// otherwise covered by the snapshot/TTL tests above.
