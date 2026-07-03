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

#include "filter.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>

namespace fs = boost::filesystem;

namespace {
std::shared_ptr<file_filter::filter_obj> make_file(const fs::path &dir, const std::string &name, const std::string &content) {
  fs::create_directories(dir);
  {
    std::ofstream ofs((dir / name).string().c_str(), std::ios::binary);
    ofs << content;
  }
  return file_filter::filter_obj::create(dir, name, content.size(), 0, 0, 0, false, 0);
}
}  // namespace

TEST(CheckFilesChecksum, KnownDigestsForHelloWorld) {
  const fs::path dir = fs::temp_directory_path() / fs::unique_path("nscp-hash-%%%%-%%%%");
  const auto f = make_file(dir, "hello.txt", "hello world");

#ifdef CHECKDISK_HAVE_OPENSSL
  EXPECT_EQ(f->get_md5(), "5eb63bbbe01eeed093cb22bb8f5acdc3");
  EXPECT_EQ(f->get_sha1(), "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed");
  EXPECT_EQ(f->get_sha256(), "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
  EXPECT_EQ(f->get_sha512(),
            "309ecc489c12d6eb4cc40f50c902f2b4d0ed77ee511a7c7a9bcd3ca86d4cd86f989dd35bc5ff499670da34255b45b0cfd830e81f605dcf7dc5542e93ae9cd76f");
  EXPECT_EQ(f->get_sha384().size(), 96u);  // 384 bits -> 96 hex chars
#else
  EXPECT_EQ(f->get_md5(), "");
  EXPECT_EQ(f->get_sha256(), "");
#endif

  fs::remove_all(dir);
}

TEST(CheckFilesChecksum, EmptyFileDigest) {
  const fs::path dir = fs::temp_directory_path() / fs::unique_path("nscp-hash-%%%%-%%%%");
  const auto f = make_file(dir, "empty.txt", "");

#ifdef CHECKDISK_HAVE_OPENSSL
  EXPECT_EQ(f->get_sha256(), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
#else
  EXPECT_EQ(f->get_sha256(), "");
#endif

  fs::remove_all(dir);
}

TEST(CheckFilesChecksum, MissingFileYieldsEmptyDigest) {
  const auto f = file_filter::filter_obj::create("/nonexistent-nscp-dir", "nope.txt", 0, 0, 0, 0, false, 0);
  EXPECT_EQ(f->get_sha256(), "");  // unreadable file -> empty, never throws
}

TEST(CheckFilesChecksum, DigestIsCached) {
  const fs::path dir = fs::temp_directory_path() / fs::unique_path("nscp-hash-%%%%-%%%%");
  const auto f = make_file(dir, "a.txt", "content");
  const std::string first = f->get_sha256();
  // Mutate the file on disk; the cached value must not change.
  {
    std::ofstream ofs((dir / "a.txt").string().c_str(), std::ios::binary);
    ofs << "different content";
  }
  EXPECT_EQ(f->get_sha256(), first);
  fs::remove_all(dir);
}
