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

#include <Windows.h>
#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>
#include <vector>

// Provide the NSCAPI singleton so any code path that logs (e.g. via
// NSC_LOG_ERROR) can be linked. The wrapper has null function pointers so
// the calls are harmless no-ops.
// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

// Forward declarations of free functions defined in check_drive.cpp.
// They are not exposed in any header but have external linkage so the
// translation units can be linked together by CMake.
extern std::string type_to_string(const long long type);
extern int do_convert_type(const std::string &keyword);
extern std::string filter_obj_filesystem_for_test(const std::string &fs);

// Magic constant used internally by check_drive.cpp to mark the synthetic
// "total" drive type. Kept in sync with check_drive.cpp.
constexpr int kDriveTypeTotal = 0x77;

// ============================================================================
// type_to_string
// ============================================================================

TEST(CheckDriveTypeToString, KnownDriveTypesMapToExpectedNames) {
  EXPECT_EQ(type_to_string(DRIVE_FIXED), "fixed");
  EXPECT_EQ(type_to_string(DRIVE_CDROM), "cdrom");
  EXPECT_EQ(type_to_string(DRIVE_REMOVABLE), "removable");
  EXPECT_EQ(type_to_string(DRIVE_REMOTE), "remote");
  EXPECT_EQ(type_to_string(DRIVE_RAMDISK), "ramdisk");
  EXPECT_EQ(type_to_string(DRIVE_UNKNOWN), "unknown");
  EXPECT_EQ(type_to_string(DRIVE_NO_ROOT_DIR), "no_root_dir");
  EXPECT_EQ(type_to_string(kDriveTypeTotal), "total");
}

TEST(CheckDriveTypeToString, UnknownTypeReturnsUnknown) {
  EXPECT_EQ(type_to_string(99999), "unknown");
  EXPECT_EQ(type_to_string(-1), "unknown");
}

// ============================================================================
// do_convert_type
// ============================================================================

TEST(CheckDriveDoConvertType, KnownKeywordsMapToDriveType) {
  EXPECT_EQ(do_convert_type("fixed"), static_cast<int>(DRIVE_FIXED));
  EXPECT_EQ(do_convert_type("cdrom"), static_cast<int>(DRIVE_CDROM));
  EXPECT_EQ(do_convert_type("removable"), static_cast<int>(DRIVE_REMOVABLE));
  EXPECT_EQ(do_convert_type("remote"), static_cast<int>(DRIVE_REMOTE));
  EXPECT_EQ(do_convert_type("ramdisk"), static_cast<int>(DRIVE_RAMDISK));
  EXPECT_EQ(do_convert_type("unknown"), static_cast<int>(DRIVE_UNKNOWN));
  EXPECT_EQ(do_convert_type("no_root_dir"), static_cast<int>(DRIVE_NO_ROOT_DIR));
  EXPECT_EQ(do_convert_type("total"), kDriveTypeTotal);
}

TEST(CheckDriveDoConvertType, UnknownKeywordReturnsMinusOne) {
  EXPECT_EQ(do_convert_type("garbage"), -1);
  EXPECT_EQ(do_convert_type(""), -1);
}

TEST(CheckDriveDoConvertType, IsCaseSensitive) {
  // The conversion is intentionally lower-case only; callers normalise the
  // input via boost::to_lower before reaching this helper.
  EXPECT_EQ(do_convert_type("FIXED"), -1);
  EXPECT_EQ(do_convert_type("Fixed"), -1);
}

// ============================================================================
// Round-trip: every known keyword resolves back to the same name string.
// ============================================================================

TEST(CheckDriveTypeRoundTrip, KeywordsAreRoundTrippable) {
  for (const std::string &kw : {"fixed", "cdrom", "removable", "remote", "ramdisk", "unknown", "no_root_dir", "total"}) {
    const int t = do_convert_type(kw);
    ASSERT_NE(t, -1) << "do_convert_type failed for: " << kw;
    EXPECT_EQ(type_to_string(t), kw) << "type_to_string mismatch for: " << kw;
  }
}

// ============================================================================
// filesystem (`fs`) keyword wiring
// ============================================================================
//
// These tests pin the round-trip from drive_container.fs (populated from
// GetVolumeInformation) through filter_obj::get_filesystem() — the value
// that backs the `filesystem` and `fs` filter keywords. The case is
// preserved verbatim from what the OS hands us; users wanting case-
// insensitive matching should use `like`.

TEST(CheckDriveFilesystem, NtfsRoundTrips) { EXPECT_EQ(filter_obj_filesystem_for_test("NTFS"), "NTFS"); }

TEST(CheckDriveFilesystem, CommonNamesRoundTrip) {
  for (const std::string &fs : {"NTFS", "FAT32", "exFAT", "ReFS", "CDFS", "UDF"}) {
    EXPECT_EQ(filter_obj_filesystem_for_test(fs), fs);
  }
}

TEST(CheckDriveFilesystem, CaseIsPreservedVerbatim) {
  // Windows reports uppercase, but we don't normalize — `like` handles
  // case-insensitive matching at filter time.
  EXPECT_EQ(filter_obj_filesystem_for_test("ntfs"), "ntfs");
  EXPECT_EQ(filter_obj_filesystem_for_test("Ntfs"), "Ntfs");
}

TEST(CheckDriveFilesystem, EmptyWhenUnknown) {
  // Unmounted / unreadable volumes yield an empty string from
  // GetVolumeInformation; that empty must propagate so users can write
  // `filesystem != ''` to filter them out.
  EXPECT_EQ(filter_obj_filesystem_for_test(""), "");
}
