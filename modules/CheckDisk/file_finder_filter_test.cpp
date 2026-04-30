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

#include <gtest/gtest.h>

#include <Windows.h>
#include <boost/shared_ptr.hpp>
#include <climits>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>

#include "file_finder.hpp"
#include "filter.hpp"

// Provide the NSCAPI singleton so any logging from file_finder.cpp /
// filter.cpp can link. The wrapper has null function pointers so the calls
// are harmless no-ops.
// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

// Forward declaration for free function defined in filter.cpp (no header
// exposes it, but it has external linkage).
extern int convert_new_type(const parsers::where::evaluation_context &context, std::string str);

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
  file_filter::filter_obj o(boost::filesystem::path("C:\\tmp"), "a.log",
                            /*now=*/100,
                            /*creationTime=*/1,
                            /*lastAccessTime=*/2,
                            /*lastWriteTime=*/3,
                            /*size=*/4096,
                            /*attributes=*/FILE_ATTRIBUTE_NORMAL);
  EXPECT_EQ(o.get_filename(), "a.log");
  EXPECT_EQ(o.get_path(), std::string("C:\\tmp"));
  EXPECT_EQ(o.get_size(), 4096u);
  EXPECT_FALSE(o.is_total());
}

TEST(FileFilterObj, ShowConcatenatesPathAndFilename) {
  file_filter::filter_obj o(boost::filesystem::path("C:\\tmp"), "a.log");
  EXPECT_EQ(o.show(), std::string("C:\\tmp\\a.log"));
}

TEST(FileFilterObj, ShowEmptyDefault) {
  file_filter::filter_obj o;
  EXPECT_EQ(o.show(), std::string("\\"));
}

// ----- get_extension --------------------------------------------------------

TEST(FileFilterObj, GetExtensionSimple) {
  file_filter::filter_obj o(boost::filesystem::path(""), "foo.txt");
  EXPECT_EQ(o.get_extension(), "txt");
}

TEST(FileFilterObj, GetExtensionNone) {
  file_filter::filter_obj o(boost::filesystem::path(""), "file");
  EXPECT_EQ(o.get_extension(), "");
}

TEST(FileFilterObj, GetExtensionTrailingDot) {
  file_filter::filter_obj o(boost::filesystem::path(""), "file.");
  EXPECT_EQ(o.get_extension(), "");
}

TEST(FileFilterObj, GetExtensionMultiDot) {
  file_filter::filter_obj o(boost::filesystem::path(""), "a.tar.gz");
  EXPECT_EQ(o.get_extension(), "gz");
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
  // The factory does not call make_total(); is_total_ stays false.
  EXPECT_FALSE(p->is_total());
}

// ----- add accumulates size -------------------------------------------------

TEST(FileFilterObj, AddAccumulatesOnlySize) {
  boost::filesystem::path p_a("p");
  boost::filesystem::path p_b("q");
  file_filter::filter_obj a(p_a, "a", 0, 0, 0, 0, /*size=*/100);
  boost::shared_ptr<file_filter::filter_obj> b(new file_filter::filter_obj(p_b, "b", 0, 0, 0, 0, /*size=*/50));
  a.add(b);
  EXPECT_EQ(a.get_size(), 150u);
  EXPECT_EQ(a.get_filename(), "a");
  EXPECT_EQ(a.get_path(), "p");
  // The summand is unchanged.
  EXPECT_EQ(b->get_size(), 50u);
}

// ----- get_type / get_type_su ----------------------------------------------

TEST(FileFilterObj, GetTypeIsDir) {
  file_filter::filter_obj o(boost::filesystem::path(""), "dir", 0, 0, 0, 0, 0,
                            /*attributes=*/FILE_ATTRIBUTE_DIRECTORY);
  EXPECT_EQ(o.get_type(), static_cast<unsigned long long>(kFileTypeDir));
  EXPECT_EQ(o.get_type_su(), "dir");
}

TEST(FileFilterObj, GetTypeIsFile) {
  file_filter::filter_obj o(boost::filesystem::path(""), "f", 0, 0, 0, 0, 0,
                            /*attributes=*/FILE_ATTRIBUTE_NORMAL);
  EXPECT_EQ(o.get_type(), static_cast<unsigned long long>(kFileTypeFile));
  EXPECT_EQ(o.get_type_su(), "file");
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

// =========================================================================
// convert_new_type  (success branches only — failure branch dereferences
// the evaluation_context and is not safely testable without the parser
// runtime.)
// =========================================================================

TEST(ConvertNewType, KeywordCritical) { EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "critical"), 1); }
TEST(ConvertNewType, KeywordError) { EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "error"), 2); }
TEST(ConvertNewType, KeywordWarning) {
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "warning"), 3);
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "warn"), 3);
}
TEST(ConvertNewType, KeywordInformational) {
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "informational"), 4);
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "info"), 4);
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "information"), 4);
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "success"), 4);
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "auditSuccess"), 4);
}
TEST(ConvertNewType, KeywordDebug) {
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "debug"), 5);
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "verbose"), 5);
}
TEST(ConvertNewType, NumericString) {
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "42"), 42);
  EXPECT_EQ(convert_new_type(parsers::where::evaluation_context{}, "0"), 0);
}


