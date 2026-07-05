// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <Windows.h>
#include <gtest/gtest.h>

#include <climits>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>

#include "file_finder.hpp"
#include "filter.hpp"

// Provide the NSCAPI singleton so any logging from file_finder.cpp /
// filter.cpp can link. The wrapper has null function pointers so the calls
// are harmless no-ops.
// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

// Mirror of file-local constants from filter.cpp.
constexpr int kFileTypeFile = 1;
constexpr int kFileTypeDir = 2;

// =========================================================================
// file_finder::is_directory
// =========================================================================

TEST(FileFinderIsDirectory, InvalidAttributesReturnsFalse) {
  // INVALID_FILE_ATTRIBUTES is (DWORD)-1.
  EXPECT_FALSE(file_finder::is_directory(static_cast<unsigned long>(-1)));
}

TEST(FileFinderIsDirectory, DirectoryFlagSet) { EXPECT_TRUE(file_finder::is_directory(FILE_ATTRIBUTE_DIRECTORY)); }

TEST(FileFinderIsDirectory, FileOnly) { EXPECT_FALSE(file_finder::is_directory(FILE_ATTRIBUTE_NORMAL)); }

TEST(FileFinderIsDirectory, MixedFlagsWithDirectory) { EXPECT_TRUE(file_finder::is_directory(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN)); }

TEST(FileFinderIsDirectory, MixedFlagsWithoutDirectory) {
  EXPECT_FALSE(file_finder::is_directory(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE));
}

// =========================================================================
// file_finder::stat_single_file (negative paths only — positive paths
// require a real file to exist on the test host. CI runners are expected
// to provide one; for the negative path it's enough to assert that a
// guaranteed-not-to-exist path returns an empty shared_ptr.)
// =========================================================================

TEST(StatSingleFile, ReturnsEmptyForMissingPath) {
  auto info = file_finder::stat_single_file(boost::filesystem::path("Z:\\nscp_test_definitely_not_a_real_path_47b1f0e5\\foo.dat"), 0);
  EXPECT_FALSE(info);
}

// =========================================================================
// file_finder::scanner_context
// =========================================================================

namespace {
file_finder::scanner_context make_ctx(int max_depth, bool debug = false) {
  file_finder::scanner_context ctx;
  ctx.debug = debug;
  ctx.pattern = "*";
  ctx.now = 0;
  ctx.max_depth = max_depth;
  return ctx;
}
}  // namespace

TEST(ScannerContext, IsValidLevelUnlimited) {
  auto ctx = make_ctx(-1);
  EXPECT_TRUE(ctx.is_valid_level(0));
  EXPECT_TRUE(ctx.is_valid_level(100));
  EXPECT_TRUE(ctx.is_valid_level(INT_MAX));
}

TEST(ScannerContext, IsValidLevelBounded) {
  auto ctx = make_ctx(3);
  EXPECT_TRUE(ctx.is_valid_level(0));
  EXPECT_TRUE(ctx.is_valid_level(1));
  EXPECT_TRUE(ctx.is_valid_level(2));
  EXPECT_FALSE(ctx.is_valid_level(3));
  EXPECT_FALSE(ctx.is_valid_level(4));
}

TEST(ScannerContext, IsValidLevelZero) {
  // max_depth=0 must mean "scan the top directory only". Previously this was
  // treated as "no levels are valid" which made check_files return "no files
  // found" when users asked for non-recursive scanning (issue #730).
  auto ctx = make_ctx(0);
  EXPECT_TRUE(ctx.is_valid_level(0));
  EXPECT_FALSE(ctx.is_valid_level(1));
  EXPECT_FALSE(ctx.is_valid_level(2));
}

TEST(ScannerContext, MissingPathsStartsEmpty) {
  // The new missing_paths vector (used to surface "path not found" as UNKNOWN
  // instead of OK / "No files found", issue #613) must default to empty so
  // that successful scans do not accidentally trip the new error path.
  auto ctx = make_ctx(1);
  EXPECT_TRUE(ctx.missing_paths.empty());
  ctx.missing_paths.push_back("X:\\does-not-exist");
  EXPECT_EQ(ctx.missing_paths.size(), 1u);
  EXPECT_EQ(ctx.missing_paths.front(), "X:\\does-not-exist");
}

TEST(ScannerContext, ReportHelpersDoNotCrash) {
  auto ctx = make_ctx(1, /*debug=*/true);
  EXPECT_NO_THROW(ctx.report_error("err"));
  EXPECT_NO_THROW(ctx.report_warning("warn"));
  EXPECT_NO_THROW(ctx.report_debug("debug-on"));

  auto ctx2 = make_ctx(1, /*debug=*/false);
  EXPECT_NO_THROW(ctx2.report_debug("debug-off"));
}

// =========================================================================
// file_filter::filter_obj
// =========================================================================

TEST(FileFilterObj, DefaultConstruction) {
  file_filter::filter_obj o;
  EXPECT_EQ(o.get_size(), 0u);
  EXPECT_FALSE(o.is_total());
  EXPECT_EQ(o.get_filename(), "");
  EXPECT_EQ(o.get_path(), "");
}

TEST(FileFilterObj, ParameterizedConstructionStoresFields) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path("C:\\tmp"), "a.log",
                                           /*size=*/4096,
                                           /*creation_time=*/1,
                                           /*access_time=*/2,
                                           /*write_time=*/3,
                                           /*is_dir=*/false,
                                           /*now=*/100);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_filename(), "a.log");
  EXPECT_EQ(o->get_path(), std::string("C:\\tmp"));
  EXPECT_EQ(o->get_size(), 4096u);
  EXPECT_FALSE(o->is_total());
}

TEST(FileFilterObj, ShowConcatenatesPathAndFilename) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path("C:\\tmp"), "a.log", 0, 0, 0, 0, /*is_dir=*/false, 0);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->show(), std::string("C:\\tmp\\a.log"));
}

TEST(FileFilterObj, ShowEmptyDefault) {
  file_filter::filter_obj o;
  // Both path and filename are empty, so joining them yields an empty string
  // (boost only inserts a separator when the left-hand side is non-empty).
  EXPECT_EQ(o.show(), std::string(""));
}

// ----- get_extension --------------------------------------------------------

TEST(FileFilterObj, GetExtensionSimple) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path(""), "foo.txt", 0, 0, 0, 0, /*is_dir=*/false, 0);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "txt");
}

TEST(FileFilterObj, GetExtensionNone) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path(""), "file", 0, 0, 0, 0, /*is_dir=*/false, 0);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "");
}

TEST(FileFilterObj, GetExtensionTrailingDot) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path(""), "file.", 0, 0, 0, 0, /*is_dir=*/false, 0);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "");
}

TEST(FileFilterObj, GetExtensionMultiDot) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path(""), "a.tar.gz", 0, 0, 0, 0, /*is_dir=*/false, 0);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "gz");
}

// ----- total flag -----------------------------------------------------------

TEST(FileFilterObj, MakeTotalSetsFlag) {
  file_filter::filter_obj o;
  EXPECT_FALSE(o.is_total());
  o.make_total();
  EXPECT_TRUE(o.is_total());
}

TEST(FileFilterObj, GetTotalFactory) {
  auto p = file_filter::filter_obj::get_total(123ULL);
  ASSERT_TRUE(p);
  EXPECT_EQ(p->get_filename(), "total");
  EXPECT_EQ(p->get_size(), 0u);
  // get_total() marks the object as the total accumulator.
  EXPECT_TRUE(p->is_total());
}

// ----- add accumulates size -------------------------------------------------

TEST(FileFilterObj, AddAccumulatesOnlySize) {
  auto a = file_filter::filter_obj::create(boost::filesystem::path("p"), "a", /*size=*/100, 0, 0, 0, /*is_dir=*/false, 0);
  auto b = file_filter::filter_obj::create(boost::filesystem::path("q"), "b", /*size=*/50, 0, 0, 0, /*is_dir=*/false, 0);
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  a->add(b);
  EXPECT_EQ(a->get_size(), 150u);
  EXPECT_EQ(a->get_filename(), "a");
  EXPECT_EQ(a->get_path(), "p");
  // The summand is unchanged.
  EXPECT_EQ(b->get_size(), 50u);
}

// ----- get_type / get_type_su ----------------------------------------------

TEST(FileFilterObj, GetTypeIsDir) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path(""), "dir", 0, 0, 0, 0, /*is_dir=*/true, 0);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_type(), static_cast<unsigned long long>(kFileTypeDir));
  EXPECT_EQ(o->get_type_su(), "dir");
}

TEST(FileFilterObj, GetTypeIsFile) {
  auto o = file_filter::filter_obj::create(boost::filesystem::path(""), "f", 0, 0, 0, 0, /*is_dir=*/false, 0);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_type(), static_cast<unsigned long long>(kFileTypeFile));
  EXPECT_EQ(o->get_type_su(), "file");
}

// =========================================================================
// file_filter::file_object_exception
// =========================================================================

TEST(FileFilterException, WhatReturnsMessage) {
  file_filter::file_object_exception e("boom");
  EXPECT_STREQ(e.what(), "boom");
}

// =========================================================================
// file_filter::filter_obj_handler  (smoke: must construct without throwing)
// =========================================================================

TEST(FileFilterHandler, ConstructsWithoutThrowing) {
  EXPECT_NO_THROW({
    file_filter::filter_obj_handler h;
    (void)&h;
  });
}
