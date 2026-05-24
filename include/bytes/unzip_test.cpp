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

#include <boost/filesystem.hpp>
#include <bytes/unzip.hpp>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <utility>

namespace fs = boost::filesystem;

namespace {

// A deterministic, deflate-compressed ZIP archive generated offline. It holds
// three entries so the tests can exercise nested paths, binary-safe content
// and zero-length entries without depending on any zip *writer* (the reader
// interface under test is read-only and platform-independent).
//
//   module.json        -> {"name":"demo","description":"A demo plugin"}
//   scripts/hello.txt  -> Hello, unzip!\n
//   empty.txt          -> (empty)
const unsigned char kZipFixture[] = {
    0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0xe3, 0x9f, 0x71, 0xd9, 0x2d, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x6d, 0x6f, 0x64, 0x75, 0x6c, 0x65, 0x2e, 0x6a, 0x73, 0x6f, 0x6e, 0xab, 0x56, 0xca, 0x4b, 0xcc, 0x4d, 0x55, 0xb2, 0x52, 0x4a, 0x49,
    0xcd, 0xcd, 0x57, 0xd2, 0x51, 0x4a, 0x49, 0x2d, 0x4e, 0x2e, 0xca, 0x2c, 0x28, 0xc9, 0xcc, 0xcf, 0x53, 0xb2, 0x52, 0x72, 0x54, 0x00, 0x09, 0x2b, 0x14, 0xe4,
    0x94, 0xa6, 0x67, 0xe6, 0x29, 0xd5, 0x02, 0x00, 0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0xcb, 0xd5, 0x17, 0x3b,
    0x10, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x73, 0x2f, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2e,
    0x74, 0x78, 0x74, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7, 0x51, 0x28, 0xcd, 0xab, 0xca, 0x2c, 0x50, 0xe4, 0x02, 0x00, 0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x65, 0x6d, 0x70,
    0x74, 0x79, 0x2e, 0x74, 0x78, 0x74, 0x03, 0x00, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0xe3, 0x9f,
    0x71, 0xd9, 0x2d, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x6d, 0x6f, 0x64, 0x75, 0x6c, 0x65, 0x2e, 0x6a, 0x73, 0x6f, 0x6e, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x21, 0x50, 0xcb, 0xd5, 0x17, 0x3b, 0x10, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x01, 0x56, 0x00, 0x00, 0x00, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x73, 0x2f, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2e, 0x74, 0x78, 0x74, 0x50, 0x4b,
    0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x21, 0x50, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x95, 0x00, 0x00, 0x00, 0x65, 0x6d, 0x70, 0x74, 0x79, 0x2e, 0x74, 0x78,
    0x74, 0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0xaf, 0x00, 0x00, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00};

auto kModuleJson = "{\"name\":\"demo\",\"description\":\"A demo plugin\"}";
auto kHelloTxt = "Hello, unzip!\n";

class UnzipTest : public ::testing::Test {
 protected:
  fs::path zip_path_;

  void SetUp() override {
    zip_path_ = fs::temp_directory_path() / fs::unique_path("nscp-unzip-%%%%-%%%%.zip");
    std::ofstream out(zip_path_.string().c_str(), std::ios::binary);
    out.write(reinterpret_cast<const char *>(kZipFixture), sizeof(kZipFixture));
    out.close();
  }

  void TearDown() override {
    boost::system::error_code ec;
    fs::remove(zip_path_, ec);
  }

  // Reads an entire on-disk file into a string (binary-safe).
  static std::string read_file(const fs::path &p) {
    std::ifstream in(p.string().c_str(), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }
};

TEST_F(UnzipTest, open_valid_archive) {
  bytes::unzip::reader r;
  ASSERT_TRUE(r.open(zip_path_.string()));
  EXPECT_TRUE(r.is_open());
  EXPECT_EQ(r.size(), 3u);
}

TEST_F(UnzipTest, constructor_opens_immediately) {
  const bytes::unzip::reader r(zip_path_.string());
  EXPECT_TRUE(r.is_open());
  EXPECT_EQ(r.size(), 3u);
}

TEST_F(UnzipTest, open_missing_file_fails) {
  bytes::unzip::reader r;
  EXPECT_FALSE(r.open((zip_path_.string() + ".does-not-exist")));
  EXPECT_FALSE(r.is_open());
  EXPECT_EQ(r.size(), 0u);
}

TEST_F(UnzipTest, default_reader_is_closed) {
  const bytes::unzip::reader r;
  EXPECT_FALSE(r.is_open());
  EXPECT_EQ(r.size(), 0u);

  bytes::unzip::file_entry e;
  EXPECT_FALSE(r.stat(0, e));

  std::string out;
  EXPECT_FALSE(r.extract("module.json", out));
}

TEST_F(UnzipTest, stat_reports_each_entry) {
  const bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());

  std::map<std::string, std::size_t> entries;
  for (unsigned int i = 0; i < r.size(); i++) {
    bytes::unzip::file_entry e;
    ASSERT_TRUE(r.stat(i, e)) << "stat failed at index " << i;
    entries[e.filename] = e.uncompressed_size;
  }

  ASSERT_EQ(entries.size(), 3u);
  ASSERT_TRUE(entries.count("module.json"));
  ASSERT_TRUE(entries.count("scripts/hello.txt"));
  ASSERT_TRUE(entries.count("empty.txt"));
  EXPECT_EQ(entries["module.json"], std::strlen(kModuleJson));
  EXPECT_EQ(entries["scripts/hello.txt"], std::strlen(kHelloTxt));
  EXPECT_EQ(entries["empty.txt"], 0u);
}

TEST_F(UnzipTest, stat_out_of_range_fails) {
  bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());
  bytes::unzip::file_entry e;
  EXPECT_FALSE(r.stat(999, e));
}

TEST_F(UnzipTest, extract_to_memory) {
  bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());

  std::string out;
  ASSERT_TRUE(r.extract("module.json", out));
  EXPECT_EQ(out, kModuleJson);

  ASSERT_TRUE(r.extract("scripts/hello.txt", out));
  EXPECT_EQ(out, kHelloTxt);
}

TEST_F(UnzipTest, extract_empty_entry) {
  const bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());

  std::string out = "not-empty-yet";
  ASSERT_TRUE(r.extract("empty.txt", out));
  EXPECT_TRUE(out.empty());
}

TEST_F(UnzipTest, extract_missing_entry_fails) {
  bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());

  std::string out;
  EXPECT_FALSE(r.extract("nope.txt", out));
}

TEST_F(UnzipTest, extract_to_file) {
  const bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());

  const fs::path dst = fs::temp_directory_path() / fs::unique_path("nscp-unzip-out-%%%%.txt");
  ASSERT_TRUE(r.extract_to_file("scripts/hello.txt", dst.string()));
  EXPECT_EQ(read_file(dst), kHelloTxt);

  boost::system::error_code ec;
  fs::remove(dst, ec);
}

TEST_F(UnzipTest, extract_to_file_missing_entry_fails) {
  const bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());

  const fs::path dst = fs::temp_directory_path() / fs::unique_path("nscp-unzip-out-%%%%.txt");
  EXPECT_FALSE(r.extract_to_file("nope.txt", dst.string()));

  boost::system::error_code ec;
  fs::remove(dst, ec);
}

TEST_F(UnzipTest, close_releases_archive) {
  bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());
  r.close();
  EXPECT_FALSE(r.is_open());
  EXPECT_EQ(r.size(), 0u);

  // Re-opening after close works.
  ASSERT_TRUE(r.open(zip_path_.string()));
  EXPECT_EQ(r.size(), 3u);
}

TEST_F(UnzipTest, reopen_switches_archive) {
  bytes::unzip::reader r(zip_path_.string());
  ASSERT_TRUE(r.is_open());
  // Re-opening a missing file leaves the reader closed rather than keeping the
  // previous handle around.
  EXPECT_FALSE(r.open(zip_path_.string() + ".missing"));
  EXPECT_FALSE(r.is_open());
}

TEST_F(UnzipTest, move_construct_transfers_ownership) {
  bytes::unzip::reader src(zip_path_.string());
  ASSERT_TRUE(src.is_open());

  const bytes::unzip::reader dst(std::move(src));
  EXPECT_TRUE(dst.is_open());
  EXPECT_EQ(dst.size(), 3u);

  std::string out;
  ASSERT_TRUE(dst.extract("module.json", out));
  EXPECT_EQ(out, kModuleJson);
}

TEST_F(UnzipTest, move_assign_transfers_ownership) {
  bytes::unzip::reader src(zip_path_.string());
  ASSERT_TRUE(src.is_open());

  bytes::unzip::reader dst;
  dst = std::move(src);
  EXPECT_TRUE(dst.is_open());
  EXPECT_EQ(dst.size(), 3u);
}

}  // namespace
