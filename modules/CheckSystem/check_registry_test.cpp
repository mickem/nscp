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

#include "check_registry.hpp"

#include <gtest/gtest.h>
#include <nsclient/nsclient_exception.hpp>
#include <win/registry.hpp>

#include <ctime>
#include <sstream>

// ============================================================================
// win_registry helpers (pure unit tests – no registry I/O)
// ============================================================================

// ── parse_hive ───────────────────────────────────────────────────────────────

TEST(WinRegistryParseHive, HKLM) {
  EXPECT_EQ(win_registry::parse_hive("HKLM"), HKEY_LOCAL_MACHINE);
  EXPECT_EQ(win_registry::parse_hive("HKEY_LOCAL_MACHINE"), HKEY_LOCAL_MACHINE);
}

TEST(WinRegistryParseHive, HKCU) {
  EXPECT_EQ(win_registry::parse_hive("HKCU"), HKEY_CURRENT_USER);
  EXPECT_EQ(win_registry::parse_hive("HKEY_CURRENT_USER"), HKEY_CURRENT_USER);
}

TEST(WinRegistryParseHive, HKCR) {
  EXPECT_EQ(win_registry::parse_hive("HKCR"), HKEY_CLASSES_ROOT);
  EXPECT_EQ(win_registry::parse_hive("HKEY_CLASSES_ROOT"), HKEY_CLASSES_ROOT);
}

TEST(WinRegistryParseHive, HKU) {
  EXPECT_EQ(win_registry::parse_hive("HKU"), HKEY_USERS);
  EXPECT_EQ(win_registry::parse_hive("HKEY_USERS"), HKEY_USERS);
}

TEST(WinRegistryParseHive, HKCC) {
  EXPECT_EQ(win_registry::parse_hive("HKCC"), HKEY_CURRENT_CONFIG);
  EXPECT_EQ(win_registry::parse_hive("HKEY_CURRENT_CONFIG"), HKEY_CURRENT_CONFIG);
}

TEST(WinRegistryParseHive, UnknownThrows) {
  EXPECT_THROW(win_registry::parse_hive("HKXYZ"), nsclient::nsclient_exception);
}

// ── hive_to_string ────────────────────────────────────────────────────────────

TEST(WinRegistryHiveToString, HKLM) { EXPECT_EQ(win_registry::hive_to_string(HKEY_LOCAL_MACHINE), "HKLM"); }
TEST(WinRegistryHiveToString, HKCU) { EXPECT_EQ(win_registry::hive_to_string(HKEY_CURRENT_USER), "HKCU"); }
TEST(WinRegistryHiveToString, HKCR) { EXPECT_EQ(win_registry::hive_to_string(HKEY_CLASSES_ROOT), "HKCR"); }
TEST(WinRegistryHiveToString, HKU)  { EXPECT_EQ(win_registry::hive_to_string(HKEY_USERS), "HKU"); }
TEST(WinRegistryHiveToString, HKCC) { EXPECT_EQ(win_registry::hive_to_string(HKEY_CURRENT_CONFIG), "HKCC"); }

// ── parse_key_path ────────────────────────────────────────────────────────────

TEST(WinRegistryParseKeyPath, SimpleHKLM) {
  const auto p = win_registry::parse_key_path("HKLM\\Software\\MyApp");
  EXPECT_EQ(p.hive_str, "HKLM");
  EXPECT_EQ(p.hive, HKEY_LOCAL_MACHINE);
  EXPECT_EQ(p.subpath, "Software\\MyApp");
}

TEST(WinRegistryParseKeyPath, HiveOnly) {
  const auto p = win_registry::parse_key_path("HKCU");
  EXPECT_EQ(p.hive_str, "HKCU");
  EXPECT_EQ(p.hive, HKEY_CURRENT_USER);
  EXPECT_TRUE(p.subpath.empty());
}

TEST(WinRegistryParseKeyPath, LongFormHive) {
  const auto p = win_registry::parse_key_path("HKEY_LOCAL_MACHINE\\System\\CurrentControlSet");
  EXPECT_EQ(p.hive_str, "HKEY_LOCAL_MACHINE");
  EXPECT_EQ(p.hive, HKEY_LOCAL_MACHINE);
  EXPECT_EQ(p.subpath, "System\\CurrentControlSet");
}

TEST(WinRegistryParseKeyPath, InvalidHiveThrows) {
  EXPECT_THROW(win_registry::parse_key_path("INVALID\\Foo\\Bar"), nsclient::nsclient_exception);
}

// ── parse_type ────────────────────────────────────────────────────────────────

TEST(WinRegistryParseType, AllKnownTypes) {
  EXPECT_EQ(win_registry::parse_type("REG_NONE"),          REG_NONE);
  EXPECT_EQ(win_registry::parse_type("REG_SZ"),            REG_SZ);
  EXPECT_EQ(win_registry::parse_type("REG_EXPAND_SZ"),     REG_EXPAND_SZ);
  EXPECT_EQ(win_registry::parse_type("REG_BINARY"),        REG_BINARY);
  EXPECT_EQ(win_registry::parse_type("REG_DWORD"),         REG_DWORD);
  EXPECT_EQ(win_registry::parse_type("REG_DWORD_BIG_ENDIAN"), REG_DWORD_BIG_ENDIAN);
  EXPECT_EQ(win_registry::parse_type("REG_LINK"),          REG_LINK);
  EXPECT_EQ(win_registry::parse_type("REG_MULTI_SZ"),      REG_MULTI_SZ);
  EXPECT_EQ(win_registry::parse_type("REG_QWORD"),         REG_QWORD);
}

TEST(WinRegistryParseType, NumericString) {
  // Numeric fallback
  EXPECT_EQ(win_registry::parse_type("4"), REG_DWORD);
}

TEST(WinRegistryParseType, InvalidThrows) {
  EXPECT_THROW(win_registry::parse_type("NOT_A_TYPE"), nsclient::nsclient_exception);
}

// ── type_to_string ────────────────────────────────────────────────────────────

TEST(WinRegistryTypeToString, AllKnownTypes) {
  EXPECT_EQ(win_registry::type_to_string(REG_NONE),      "REG_NONE");
  EXPECT_EQ(win_registry::type_to_string(REG_SZ),        "REG_SZ");
  EXPECT_EQ(win_registry::type_to_string(REG_EXPAND_SZ), "REG_EXPAND_SZ");
  EXPECT_EQ(win_registry::type_to_string(REG_BINARY),    "REG_BINARY");
  EXPECT_EQ(win_registry::type_to_string(REG_DWORD),     "REG_DWORD");
  EXPECT_EQ(win_registry::type_to_string(REG_MULTI_SZ),  "REG_MULTI_SZ");
  EXPECT_EQ(win_registry::type_to_string(REG_QWORD),     "REG_QWORD");
}

TEST(WinRegistryTypeToString, UnknownType) {
  // Should not throw; should return something containing the numeric value
  const std::string s = win_registry::type_to_string(0xDEAD);
  EXPECT_NE(s.find("UNKNOWN"), std::string::npos);
}

// ── parse_view ────────────────────────────────────────────────────────────────

TEST(WinRegistryParseView, Default) { EXPECT_EQ(win_registry::parse_view("default"), 0u); }
TEST(WinRegistryParseView, View32)  { EXPECT_EQ(win_registry::parse_view("32"),  KEY_WOW64_32KEY); }
TEST(WinRegistryParseView, View64)  { EXPECT_EQ(win_registry::parse_view("64"),  KEY_WOW64_64KEY); }
TEST(WinRegistryParseView, Empty)   { EXPECT_EQ(win_registry::parse_view(""),    0u); }

// ── key_info struct ───────────────────────────────────────────────────────────

TEST(WinRegistryKeyInfo, DefaultConstruction) {
  win_registry::key_info ki;
  EXPECT_TRUE(ki.path.empty());
  EXPECT_TRUE(ki.name.empty());
  EXPECT_FALSE(ki.exists);
  EXPECT_EQ(ki.depth, 0);
  EXPECT_EQ(ki.value_count, 0);
  EXPECT_EQ(ki.subkey_count, 0);
  EXPECT_EQ(ki.written_ft, 0u);
  EXPECT_EQ(ki.get_exists(), 0);
}

TEST(WinRegistryKeyInfo, ExistsGetters) {
  win_registry::key_info ki;
  ki.exists = true;
  ki.path   = "HKLM\\Software\\Test";
  ki.name   = "Test";
  ki.parent = "HKLM\\Software";
  ki.hive   = "HKLM";
  ki.depth  = 1;
  ki.value_count  = 3;
  ki.subkey_count = 2;

  EXPECT_EQ(ki.get_path(),        "HKLM\\Software\\Test");
  EXPECT_EQ(ki.get_name(),        "Test");
  EXPECT_EQ(ki.get_parent(),      "HKLM\\Software");
  EXPECT_EQ(ki.get_hive(),        "HKLM");
  EXPECT_EQ(ki.get_depth(),       1);
  EXPECT_EQ(ki.get_exists(),      1);
  EXPECT_EQ(ki.get_value_count(), 3);
  EXPECT_EQ(ki.get_subkey_count(),2);
  EXPECT_EQ(ki.show(),            "HKLM\\Software\\Test");
}

TEST(WinRegistryKeyInfo, AgeWhenFtIsZero) {
  win_registry::key_info ki;
  ki.written_ft = 0;
  EXPECT_EQ(ki.get_written(), 0);
  EXPECT_EQ(ki.get_age(), 0);
}

TEST(WinRegistryKeyInfo, AgeIsNonNegative) {
  // Use a FILETIME that corresponds to the epoch (Jan 1, 1970)
  // FILETIME for Unix epoch = 116444736000000000 (100-ns intervals from 1601-01-01)
  constexpr unsigned long long epoch_ft = 116444736000000000ULL;
  win_registry::key_info ki;
  ki.written_ft = epoch_ft;
  EXPECT_GE(ki.get_age(), 0);
}

// ── value_info struct ─────────────────────────────────────────────────────────

TEST(WinRegistryValueInfo, DefaultConstruction) {
  win_registry::value_info vi;
  EXPECT_TRUE(vi.key.empty());
  EXPECT_TRUE(vi.name.empty());
  EXPECT_FALSE(vi.exists);
  EXPECT_EQ(vi.type, static_cast<DWORD>(REG_NONE));
  EXPECT_EQ(vi.int_value, 0);
  EXPECT_EQ(vi.size, 0);
}

TEST(WinRegistryValueInfo, DefaultValueName) {
  win_registry::value_info vi;
  vi.name = "";
  EXPECT_EQ(vi.get_name(), "(default)");
}

TEST(WinRegistryValueInfo, NamedValue) {
  win_registry::value_info vi;
  vi.name = "Shell";
  EXPECT_EQ(vi.get_name(), "Shell");
}

TEST(WinRegistryValueInfo, TypeGetters) {
  win_registry::value_info vi;
  vi.type = REG_SZ;
  EXPECT_EQ(vi.get_type_i(), static_cast<long long>(REG_SZ));
  EXPECT_EQ(vi.get_type_s(), "REG_SZ");

  vi.type = REG_DWORD;
  EXPECT_EQ(vi.get_type_s(), "REG_DWORD");
}

TEST(WinRegistryValueInfo, StringValue) {
  win_registry::value_info vi;
  vi.type         = REG_SZ;
  vi.string_value = "explorer.exe";
  vi.int_value    = 0;
  vi.size         = 26;
  vi.exists       = true;

  EXPECT_EQ(vi.get_string_value(), "explorer.exe");
  EXPECT_EQ(vi.get_size(), 26);
  EXPECT_EQ(vi.get_exists(), 1);
  EXPECT_NE(vi.show().find("explorer.exe"), std::string::npos);
}

TEST(WinRegistryValueInfo, IntValue) {
  win_registry::value_info vi;
  vi.type      = REG_DWORD;
  vi.int_value = 42;
  vi.string_value = "42";
  vi.exists = true;

  EXPECT_EQ(vi.get_int_value(), 42);
}

TEST(WinRegistryValueInfo, ParseTypeS) {
  EXPECT_EQ(win_registry::value_info::parse_type_s("REG_SZ"),    static_cast<long long>(REG_SZ));
  EXPECT_EQ(win_registry::value_info::parse_type_s("REG_DWORD"), static_cast<long long>(REG_DWORD));
}

// ============================================================================
// Integration tests – actual registry I/O via HKCU (always available)
// These use a temporary test key under HKCU\Software\NSCP_test_<pid>.
// ============================================================================

namespace {
// Build a test key path unique to this process run
std::string test_base_key() {
  std::ostringstream oss;
  oss << "HKCU\\Software\\NSCP_test_" << static_cast<unsigned long>(GetCurrentProcessId());
  return oss.str();
}

// RAII helper: creates a registry key tree for tests and deletes it on destruction
struct TestRegistryFixture {
  std::string base_path;      // e.g. "HKCU\Software\NSCP_test_1234"
  std::string base_subpath;   // e.g. "Software\NSCP_test_1234"
  bool setup_ok = false;

  TestRegistryFixture() {
    base_path    = test_base_key();
    const auto p = win_registry::parse_key_path(base_path);
    base_subpath = p.subpath;

    // Create base key
    HKEY hKey = NULL;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, utf8::cvt<std::wstring>(base_subpath).c_str(),
                        0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hKey, &disp) != ERROR_SUCCESS) {
      return;
    }

    // Write a REG_SZ value
    const std::wstring val_sz = L"HelloWorld";
    RegSetValueExW(hKey, L"TestSZ", 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(val_sz.c_str()),
                   static_cast<DWORD>((val_sz.size() + 1) * sizeof(wchar_t)));

    // Write a REG_DWORD value
    DWORD val_dw = 42;
    RegSetValueExW(hKey, L"TestDWORD", 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&val_dw), sizeof(DWORD));

    RegCloseKey(hKey);

    // Create a sub-key
    HKEY hSub = NULL;
    const std::string sub_subpath = base_subpath + "\\SubKey";
    RegCreateKeyExW(HKEY_CURRENT_USER, utf8::cvt<std::wstring>(sub_subpath).c_str(),
                    0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &hSub, &disp);
    if (hSub) RegCloseKey(hSub);

    setup_ok = true;
  }

  ~TestRegistryFixture() {
    // Delete the whole test tree
    RegDeleteTreeW(HKEY_CURRENT_USER, utf8::cvt<std::wstring>(base_subpath).c_str());
  }
};
}  // anonymous namespace

// ── enum_values ───────────────────────────────────────────────────────────────

TEST(CheckRegistryIntegration, EnumValues_ReturnsExpectedValues) {
  TestRegistryFixture fix;
  if (!fix.setup_ok) GTEST_SKIP() << "Registry fixture setup failed";

  const auto vals = win_registry::enum_values(HKEY_CURRENT_USER, fix.base_subpath, fix.base_path, "HKCU", 0);
  EXPECT_GE(vals.size(), 2u);

  bool found_sz = false, found_dw = false;
  for (const auto &v : vals) {
    if (v.name == "TestSZ") {
      found_sz = true;
      EXPECT_EQ(v.type, static_cast<DWORD>(REG_SZ));
      EXPECT_EQ(v.string_value, "HelloWorld");
      EXPECT_TRUE(v.exists);
      EXPECT_EQ(v.hive, "HKCU");
    }
    if (v.name == "TestDWORD") {
      found_dw = true;
      EXPECT_EQ(v.type, static_cast<DWORD>(REG_DWORD));
      EXPECT_EQ(v.int_value, 42);
      EXPECT_EQ(v.string_value, "42");
      EXPECT_TRUE(v.exists);
    }
  }
  EXPECT_TRUE(found_sz);
  EXPECT_TRUE(found_dw);
}

// ── enum_sub_keys ─────────────────────────────────────────────────────────────

TEST(CheckRegistryIntegration, EnumSubKeys_FindsSubKey) {
  TestRegistryFixture fix;
  if (!fix.setup_ok) GTEST_SKIP() << "Registry fixture setup failed";

  const auto kids = win_registry::enum_sub_keys(HKEY_CURRENT_USER, fix.base_subpath, fix.base_path, "HKCU", 1, 0);
  EXPECT_GE(kids.size(), 1u);

  bool found = false;
  for (const auto &k : kids) {
    if (k.name == "SubKey") {
      found = true;
      EXPECT_EQ(k.hive, "HKCU");
      EXPECT_EQ(k.depth, 1);
      EXPECT_TRUE(k.exists);
      EXPECT_EQ(k.parent, fix.base_path);
    }
  }
  EXPECT_TRUE(found);
}

// ── open_key – existing key ───────────────────────────────────────────────────

TEST(CheckRegistryIntegration, OpenKey_ExistingKey) {
  TestRegistryFixture fix;
  if (!fix.setup_ok) GTEST_SKIP() << "Registry fixture setup failed";

  const auto p = win_registry::parse_key_path(fix.base_path);
  win_registry::key_info ki = win_registry::open_key(
      HKEY_CURRENT_USER, p.subpath, fix.base_path,
      "NSCP_test_key", "HKCU\\Software", "HKCU", 0, 0);

  EXPECT_TRUE(ki.exists);
  EXPECT_GE(ki.value_count, 2);     // TestSZ + TestDWORD
  EXPECT_GE(ki.subkey_count, 1);    // SubKey
  EXPECT_GT(ki.written_ft, 0u);
}

// ── open_key – missing key ────────────────────────────────────────────────────

TEST(CheckRegistryIntegration, OpenKey_MissingKey) {
  const std::string missing = "HKCU\\Software\\NSCP_definitely_does_not_exist_12345";
  const auto p = win_registry::parse_key_path(missing);
  win_registry::key_info ki = win_registry::open_key(
      HKEY_CURRENT_USER, p.subpath, missing,
      "NSCP_definitely_does_not_exist_12345", "HKCU\\Software", "HKCU", 0, 0);

  EXPECT_FALSE(ki.exists);
  EXPECT_EQ(ki.get_exists(), 0);
}

// ── recursive_enum_keys ───────────────────────────────────────────────────────

TEST(CheckRegistryIntegration, RecursiveEnumKeys_MaxDepth0) {
  TestRegistryFixture fix;
  if (!fix.setup_ok) GTEST_SKIP() << "Registry fixture setup failed";

  // max_depth=0 → no results (children at depth=1, but depth > max_depth=0 is skipped)
  std::vector<win_registry::key_info> out;
  win_registry::recursive_enum_keys(HKEY_CURRENT_USER, fix.base_subpath, fix.base_path, "HKCU", 1, 0, 0, out);
  EXPECT_TRUE(out.empty());
}

TEST(CheckRegistryIntegration, RecursiveEnumKeys_Unlimited) {
  TestRegistryFixture fix;
  if (!fix.setup_ok) GTEST_SKIP() << "Registry fixture setup failed";

  std::vector<win_registry::key_info> out;
  win_registry::recursive_enum_keys(HKEY_CURRENT_USER, fix.base_subpath, fix.base_path, "HKCU", 1, -1, 0, out);
  EXPECT_GE(out.size(), 1u);

  bool found_sub = false;
  for (const auto &k : out) {
    if (k.name == "SubKey") found_sub = true;
  }
  EXPECT_TRUE(found_sub);
}

// ── age and written time ──────────────────────────────────────────────────────

TEST(CheckRegistryIntegration, WrittenTimeIsRecentForNewKey) {
  TestRegistryFixture fix;
  if (!fix.setup_ok) GTEST_SKIP() << "Registry fixture setup failed";

  const auto p = win_registry::parse_key_path(fix.base_path);
  win_registry::key_info ki = win_registry::open_key(
      HKEY_CURRENT_USER, p.subpath, fix.base_path,
      "test", "HKCU\\Software", "HKCU", 0, 0);

  ASSERT_TRUE(ki.exists);
  EXPECT_GT(ki.get_written(), 0);

  // The key was just created, so age should be very small (< 60 seconds)
  const long long age = ki.get_age();
  EXPECT_GE(age, 0);
  EXPECT_LT(age, 60);
}

TEST(CheckRegistryIntegration, ValueWrittenTimeMatchesKeyWrittenTime) {
  TestRegistryFixture fix;
  if (!fix.setup_ok) GTEST_SKIP() << "Registry fixture setup failed";

  const auto vals = win_registry::enum_values(HKEY_CURRENT_USER, fix.base_subpath, fix.base_path, "HKCU", 0);
  for (const auto &v : vals) {
    EXPECT_GT(v.written_ft, 0u) << "Value " << v.name << " should have a non-zero written_ft";
    EXPECT_GT(v.get_written(), 0) << "Value " << v.name << " should have a non-zero epoch written time";
  }
}
