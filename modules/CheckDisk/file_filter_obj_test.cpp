// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Platform-neutral tests for file_filter::filter_obj (filter.hpp/filter.cpp).
// The Windows build exercises these accessors through file_finder_filter_test;
// this file gives the unix test target the same coverage of the shared
// accessors, the derived/formatted fields and the aggregate (total) object.

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <cstdio>
#include <fstream>
#include <memory>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <string>

#include "filter.hpp"

// nscapi::plugin_singleton is defined once in check_disk_unix_test.cpp for the
// merged check_disk_unix_test target.

namespace {

// Mirror of file-local constants from filter.cpp.
constexpr int kFileTypeFile = 1;
constexpr int kFileTypeDir = 2;

file_filter::filter_obj_ptr make_obj(const std::string& dir, const std::string& name, unsigned long long size = 0, long long creation = 0,
                                     long long access = 0, long long write = 0, bool is_dir = false) {
  return file_filter::filter_obj::create(boost::filesystem::path(dir), name, size, creation, access, write, is_dir, /*now=*/1000);
}

// RAII temp file with fixed content, removed on destruction.
struct temp_file {
  boost::filesystem::path path;
  explicit temp_file(const std::string& name, const std::string& content) {
    path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp_filter_obj_test_%%%%%%%%");
    boost::filesystem::create_directories(path);
    path /= name;
    std::ofstream out(path.string().c_str(), std::ios::binary);
    out << content;
  }
  ~temp_file() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(path.parent_path(), ec);
  }
};

}  // namespace

// ----- basic accessors ------------------------------------------------------

TEST(FilterObjAccessors, PathFilenameAndShow) {
  auto o = make_obj("/tmp/some/dir", "a.log", 4096);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_filename(), "a.log");
  EXPECT_EQ(o->get_path(), "/tmp/some/dir");
  EXPECT_EQ(o->show(), "/tmp/some/dir/a.log");
  EXPECT_EQ(o->get_size(), 4096u);
  EXPECT_FALSE(o->is_total());
}

TEST(FilterObjAccessors, TimestampsRoundTrip) {
  auto o = make_obj("/tmp", "f", 0, /*creation=*/11, /*access=*/22, /*write=*/33);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_creation(), 11);
  EXPECT_EQ(o->get_access(), 22);
  EXPECT_EQ(o->get_write(), 33);
}

// ----- formatted timestamps -------------------------------------------------

TEST(FilterObjFormatting, UtcFormattersRenderEpochTimes) {
  // 86400 = 1970-01-02 00:00:00 UTC; the _su formatters are UTC so the
  // rendered strings are deterministic regardless of the host timezone.
  auto o = make_obj("/tmp", "f", 0, /*creation=*/86400, /*access=*/86400 + 3600, /*write=*/86400 + 60);
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_creation_su(), "1970-01-02 00:00:00");
  EXPECT_EQ(o->get_access_su(), "1970-01-02 01:00:00");
  EXPECT_EQ(o->get_written_su(), "1970-01-02 00:01:00");
}

TEST(FilterObjFormatting, LocalFormattersRenderDateShape) {
  // The _sl formatters use the host timezone, so only assert on the shape
  // ("YYYY-MM-DD HH:MM:SS") and the year, which any timezone keeps in 1970.
  auto o = make_obj("/tmp", "f", 0, /*creation=*/86400, /*access=*/86400, /*write=*/86400);
  ASSERT_TRUE(o);
  for (const std::string& s : {o->get_creation_sl(), o->get_access_sl(), o->get_written_sl()}) {
    ASSERT_EQ(s.size(), 19u) << s;
    EXPECT_EQ(s.substr(0, 4), "1970") << s;
    EXPECT_EQ(s[4], '-') << s;
    EXPECT_EQ(s[7], '-') << s;
    EXPECT_EQ(s[10], ' ') << s;
    EXPECT_EQ(s[13], ':') << s;
    EXPECT_EQ(s[16], ':') << s;
  }
  EXPECT_EQ(file_filter::filter_obj::format_local(86400).substr(0, 4), "1970");
}

// ----- get_extension --------------------------------------------------------

TEST(FilterObjExtension, SimpleExtension) {
  auto o = make_obj("/tmp", "foo.txt");
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "txt");
}

TEST(FilterObjExtension, NoExtension) {
  auto o = make_obj("/tmp", "file");
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "");
}

TEST(FilterObjExtension, TrailingDot) {
  auto o = make_obj("/tmp", "file.");
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "");
}

TEST(FilterObjExtension, MultiDotUsesLastComponent) {
  auto o = make_obj("/tmp", "a.tar.gz");
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_extension(), "gz");
}

// ----- type keywords --------------------------------------------------------

TEST(FilterObjType, DirAndFile) {
  auto d = make_obj("/tmp", "sub", 0, 0, 0, 0, /*is_dir=*/true);
  auto f = make_obj("/tmp", "f", 0, 0, 0, 0, /*is_dir=*/false);
  ASSERT_TRUE(d);
  ASSERT_TRUE(f);
  EXPECT_EQ(d->get_type(), static_cast<unsigned long long>(kFileTypeDir));
  EXPECT_EQ(d->get_type_su(), "dir");
  EXPECT_EQ(f->get_type(), static_cast<unsigned long long>(kFileTypeFile));
  EXPECT_EQ(f->get_type_su(), "file");
}

// ----- total object / aggregates -------------------------------------------

TEST(FilterObjTotal, GetTotalFactoryMarksTotal) {
  auto total = file_filter::filter_obj::get_total(123ULL);
  ASSERT_TRUE(total);
  EXPECT_TRUE(total->is_total());
  EXPECT_EQ(total->get_filename(), "total");
  EXPECT_EQ(total->get_size(), 0u);
}

TEST(FilterObjTotal, MakeTotalSetsFlag) {
  file_filter::filter_obj o;
  EXPECT_FALSE(o.is_total());
  o.make_total();
  EXPECT_TRUE(o.is_total());
}

TEST(FilterObjTotal, AddAccumulatesSizeAndAggregates) {
  auto total = file_filter::filter_obj::get_total(0);
  total->add(make_obj("/d", "f1", 100));
  total->add(make_obj("/d", "f2", 500));
  total->add(make_obj("/d", "f3", 300));
  total->add(make_obj("/d", "sub", 0, 0, 0, 0, /*is_dir=*/true));

  EXPECT_EQ(total->get_size(), 900u);          // 100 + 500 + 300 + 0
  EXPECT_EQ(total->get_smallest_size(), 0u);   // the folder (size 0)
  EXPECT_EQ(total->get_largest_size(), 500u);
  EXPECT_EQ(total->get_average_size(), 225u);  // 900 / 4
  EXPECT_EQ(total->get_folder_count(), 1);     // one dir
}

TEST(FilterObjTotal, AggregatesAreZeroWhenEmpty) {
  auto total = file_filter::filter_obj::get_total(0);
  EXPECT_EQ(total->get_smallest_size(), 0u);
  EXPECT_EQ(total->get_largest_size(), 0u);
  EXPECT_EQ(total->get_average_size(), 0u);  // guarded divide-by-zero
  EXPECT_EQ(total->get_folder_count(), 0);
}

// ----- filesystem-backed derived fields ------------------------------------

TEST(FilterObjContent, LineCountReadsRealFileAndCaches) {
  temp_file f("lines.txt", "one\ntwo\nthree\n");
  auto o = make_obj(f.path.parent_path().string(), f.path.filename().string());
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_line_count(), 3u);
  // Cached: the count survives even if the file disappears.
  boost::filesystem::remove(f.path);
  EXPECT_EQ(o->get_line_count(), 3u);
}

TEST(FilterObjContent, LineCountMissingFileIsZero) {
  auto o = make_obj("/nonexistent-nscp-test-dir", "missing.txt");
  ASSERT_TRUE(o);
  EXPECT_EQ(o->get_line_count(), 0u);
}

TEST(FilterObjVersion, VersionIsEmptyOnUnix) {
  // PE file versions are a Windows concept; the unix implementation always
  // yields an empty string, even for a real file.
  temp_file f("some.bin", "not a PE file");
  auto o = make_obj(f.path.parent_path().string(), f.path.filename().string());
  ASSERT_TRUE(o);
  parsers::where::evaluation_context ctx;
  EXPECT_EQ(o->get_version(ctx), "");
}

// ----- exception type -------------------------------------------------------

TEST(FilterObjException, WhatReturnsMessage) {
  file_filter::file_object_exception e("boom");
  EXPECT_STREQ(e.what(), "boom");
}
