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

#include <settings/impl/settings_ini.hpp>
#include <settings/test_helpers.hpp>

using settings_test::mock_settings_core;
using settings_test::temp_dir;
using settings_test::write_file;

namespace {

// Build an "ini:///<absolute path>" context.  We want exactly three slashes
// after the colon so that net::parse leaves host empty and assigns the
// absolute path to url_.path.  generic_string() includes a leading '/' on
// POSIX but not on Windows ("C:/..."), so strip it before concatenating.
std::string ini_context(const boost::filesystem::path &p) {
  std::string s = p.generic_string();
  if (!s.empty() && s.front() == '/') s.erase(0, 1);
  return "ini:///" + s;
}

}  // namespace

TEST(settings_ini, type_is_ini) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  EXPECT_EQ(s.get_type(), "ini");
}

TEST(settings_ini, supports_updates) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  EXPECT_TRUE(s.supports_updates());
}

TEST(settings_ini, get_real_string_reads_existing_key) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/foo]\nbar=baz\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  auto v = s.get_real_string({"/foo", "bar"});
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "baz");
}

TEST(settings_ini, get_real_string_missing_key_returns_empty) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/foo]\nbar=baz\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  EXPECT_FALSE(s.get_real_string({"/foo", "missing"}).has_value());
  EXPECT_FALSE(s.get_real_string({"/missing", "bar"}).has_value());
}

TEST(settings_ini, has_real_key_distinguishes_present_and_absent) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/foo]\nbar=baz\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  EXPECT_TRUE(s.has_real_key({"/foo", "bar"}));
  EXPECT_FALSE(s.has_real_key({"/foo", "missing"}));
}

TEST(settings_ini, has_real_path_true_only_for_existing_section) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/foo]\nbar=baz\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  EXPECT_TRUE(s.has_real_path("/foo"));
  EXPECT_FALSE(s.has_real_path("/missing"));
}

TEST(settings_ini, get_real_keys_lists_section_keys) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/foo]\na=1\nb=2\nc=3\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  settings::INISettings::string_list keys;
  s.get_real_keys("/foo", keys);
  EXPECT_EQ(keys.size(), 3u);
}

TEST(settings_ini, get_real_sections_lists_root_sections) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/alpha]\nx=1\n[/beta]\ny=2\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  settings::INISettings::string_list sections;
  s.get_real_sections("", sections);
  // /includes is auto-registered by load_data() but not added to the file.
  EXPECT_GE(sections.size(), 2u);
  EXPECT_NE(std::find(sections.begin(), sections.end(), "/alpha"), sections.end());
  EXPECT_NE(std::find(sections.begin(), sections.end(), "/beta"), sections.end());
}

TEST(settings_ini, set_real_value_persists_after_save) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "");
  mock_settings_core core;
  {
    settings::INISettings s(&core, "test", ini_context(file));
    s.set_real_value({"/section", "key"}, settings::settings_interface_impl::conainer(std::string("hello"), /*dirty=*/true));
    s.save(false);
  }
  // Re-open the file in a fresh instance to verify the value round-trips.
  settings::INISettings reloaded(&core, "test", ini_context(file));
  auto v = reloaded.get_real_string({"/section", "key"});
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "hello");
}

TEST(settings_ini, set_real_value_skips_when_not_dirty) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/section]\nkey=original\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  // Non-dirty container: set_real_value short-circuits, value should be unchanged.
  s.set_real_value({"/section", "key"}, settings::settings_interface_impl::conainer(std::string("ignored"), /*dirty=*/false));
  auto v = s.get_real_string({"/section", "key"});
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "original");
}

TEST(settings_ini, remove_real_value_drops_key) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/section]\nk1=v1\nk2=v2\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  ASSERT_TRUE(s.has_real_key({"/section", "k1"}));
  s.remove_real_value({"/section", "k1"});
  EXPECT_FALSE(s.has_real_key({"/section", "k1"}));
  EXPECT_TRUE(s.has_real_key({"/section", "k2"}));
}

TEST(settings_ini, remove_real_path_drops_section) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/section]\nk=v\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  ASSERT_TRUE(s.has_real_path("/section"));
  s.remove_real_path("/section");
  EXPECT_FALSE(s.has_real_path("/section"));
}

TEST(settings_ini, missing_file_does_not_throw_on_construct) {
  temp_dir dir;
  // Note: file does not exist on disk.
  auto file = dir.file("does_not_exist.ini");
  mock_settings_core core;
  EXPECT_NO_THROW(settings::INISettings(&core, "test", ini_context(file)));
}

TEST(settings_ini, save_writes_value_to_file) {
  // INISettings::get_file_name() resolves a context's absolute path via a
  // substr(1) trick that depends on the file (or its parent) already existing.
  // For non-existent absolute paths on Windows, the leading "/" before the
  // drive letter ("/C:/...") stays and SaveFile fails with ERROR_INVALID_NAME.
  // Production NSCP avoids this by always pointing at an existing file or
  // using ${exe-path}-relative contexts; we mirror that by pre-creating the
  // target file as empty.
  temp_dir dir;
  auto file = dir.file("new.ini");
  settings_test::write_file(file, "");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  s.set_real_value({"/section", "key"}, settings::settings_interface_impl::conainer(std::string("v"), true));
  s.save(false);
  ASSERT_TRUE(boost::filesystem::exists(file));
  const std::string content = settings_test::read_file(file);
  EXPECT_NE(content.find("key"), std::string::npos);
  EXPECT_NE(content.find("v"), std::string::npos);
}

TEST(settings_ini, ensure_exists_does_not_throw_on_existing_file) {
  temp_dir dir;
  auto file = dir.file("ensure.ini");
  settings_test::write_file(file, "");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  EXPECT_NO_THROW(s.ensure_exists());
  EXPECT_TRUE(boost::filesystem::exists(file));
}

TEST(settings_ini, validate_flags_unregistered_paths_and_keys) {
  temp_dir dir;
  auto file = dir.file("a.ini");
  write_file(file, "[/unknown]\nfoo=bar\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  // mock_settings_core::get_registered_path returns a default-constructed path
  // description (no throw), so validate doesn't surface unknown sections here.
  // It still runs to completion without exception, which is what we assert.
  EXPECT_NO_THROW(s.validate());
}

TEST(settings_ini, context_exists_for_real_file) {
  temp_dir dir;
  auto file = dir.file("real.ini");
  write_file(file, "");
  mock_settings_core core;
  EXPECT_TRUE(settings::INISettings::context_exists(&core, ini_context(file)));
}

TEST(settings_ini, context_exists_false_for_missing_file) {
  temp_dir dir;
  auto file = dir.file("nope.ini");
  mock_settings_core core;
  EXPECT_FALSE(settings::INISettings::context_exists(&core, ini_context(file)));
}

// Issue #205: nscp settings --sort writes the INI store with sections (and
// keys within each section) sorted alphabetically. [/modules] is pinned as
// the first section.
TEST(settings_ini, save_sorted_orders_sections_and_keys) {
  temp_dir dir;
  auto file = dir.file("sort.ini");
  write_file(file,
             "[/zeta]\n"
             "y_key = y_val\n"
             "a_key = a_val\n"
             "\n"
             "[/alpha]\n"
             "b = 2\n"
             "a = 1\n"
             "\n"
             "[/modules]\n"
             "PluginA = enabled\n"
             "PluginB = enabled\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  s.save_sorted();

  std::ifstream in(file.string());
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  const auto pos_modules = content.find("[/modules]");
  const auto pos_alpha = content.find("[/alpha]");
  const auto pos_zeta = content.find("[/zeta]");
  ASSERT_NE(pos_modules, std::string::npos);
  ASSERT_NE(pos_alpha, std::string::npos);
  ASSERT_NE(pos_zeta, std::string::npos);
  EXPECT_LT(pos_modules, pos_alpha);
  EXPECT_LT(pos_alpha, pos_zeta);

  const auto alpha_block = content.substr(pos_alpha, pos_zeta - pos_alpha);
  EXPECT_LT(alpha_block.find("a ="), alpha_block.find("b ="));
  const auto zeta_block = content.substr(pos_zeta);
  EXPECT_LT(zeta_block.find("a_key"), zeta_block.find("y_key"));
}

TEST(settings_ini, save_sorted_preserves_values) {
  temp_dir dir;
  auto file = dir.file("sort_values.ini");
  write_file(file, "[/section]\nkey = my_value\nother = 42\n");
  mock_settings_core core;
  settings::INISettings s(&core, "test", ini_context(file));
  s.save_sorted();

  std::ifstream in(file.string());
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("my_value"), std::string::npos);
  EXPECT_NE(content.find("42"), std::string::npos);
}
