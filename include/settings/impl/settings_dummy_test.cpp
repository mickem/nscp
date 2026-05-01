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

#include <settings/impl/settings_dummy.hpp>
#include <settings/test_helpers.hpp>

using settings_test::mock_settings_core;

// settings_interface_impl owns a boost::timed_mutex (non-copyable / non-movable),
// so each test constructs settings_dummy in place rather than going through a
// returns-by-value helper.

TEST(settings_dummy, type_is_dummy) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_EQ(d.get_type(), "dummy");
}

TEST(settings_dummy, info_is_descriptive) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_EQ(d.get_info(), "dummy settings");
}

TEST(settings_dummy, does_not_support_updates) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_FALSE(d.supports_updates());
}

TEST(settings_dummy, get_real_string_returns_empty) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_FALSE(d.get_real_string({"/path", "key"}).has_value());
}

TEST(settings_dummy, get_real_int_returns_empty) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_FALSE(d.get_real_int({"/path", "key"}).has_value());
}

TEST(settings_dummy, get_real_bool_returns_empty) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_FALSE(d.get_real_bool({"/path", "key"}).has_value());
}

TEST(settings_dummy, has_real_key_always_false) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_FALSE(d.has_real_key({"/anything", "anything"}));
}

TEST(settings_dummy, has_real_path_always_false) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_FALSE(d.has_real_path("/anything"));
}

TEST(settings_dummy, get_real_sections_leaves_list_unchanged) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  settings::settings_dummy::string_list out;
  out.push_back("preexisting");
  d.get_real_sections("/path", out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out.front(), "preexisting");
}

TEST(settings_dummy, get_real_keys_leaves_list_unchanged) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  settings::settings_dummy::string_list out;
  d.get_real_keys("/path", out);
  EXPECT_TRUE(out.empty());
}

TEST(settings_dummy, validate_returns_no_errors) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_TRUE(d.validate().empty());
}

TEST(settings_dummy, context_exists_always_true) {
  mock_settings_core core;
  EXPECT_TRUE(settings::settings_dummy::context_exists(&core, "anything"));
  EXPECT_TRUE(settings::settings_dummy::context_exists(&core, ""));
}

TEST(settings_dummy, set_real_value_is_noop) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  // Just verify it doesn't throw — there's no observable state to check.
  EXPECT_NO_THROW(d.set_real_value({"/x", "y"}, settings::settings_interface_impl::conainer(std::string("v"), true)));
}

TEST(settings_dummy, remove_real_value_is_noop) {
  mock_settings_core core;
  settings::settings_dummy d(&core, "test", "dummy://");
  EXPECT_NO_THROW(d.remove_real_value({"/x", "y"}));
  EXPECT_NO_THROW(d.remove_real_path("/x"));
}
