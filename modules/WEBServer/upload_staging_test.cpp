// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "upload_staging.hpp"

#include <gtest/gtest.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = boost::filesystem;

namespace {

// A private directory for each test, removed afterwards whatever happened.
struct scratch_dir {
  fs::path path;
  scratch_dir() : path(fs::temp_directory_path() / fs::unique_path("nscp-upload-staging-test-%%%%%%%%")) { fs::create_directories(path); }
  ~scratch_dir() {
    boost::system::error_code ec;
    fs::remove_all(path, ec);
  }
};

std::string read_all(const fs::path &path) {
  std::ifstream in(path.string().c_str(), std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(upload_staging, stages_the_content_under_a_random_name_in_the_directory) {
  scratch_dir dir;
  std::string error;

  const fs::path staged = upload_staging::stage(dir.path, "#!/bin/sh\necho hello\n", error);

  ASSERT_FALSE(staged.empty()) << error;
  EXPECT_TRUE(error.empty()) << error;
  EXPECT_EQ(staged.parent_path(), dir.path);
  EXPECT_TRUE(boost::algorithm::starts_with(staged.filename().string(), "nscp-upload-")) << staged;
  EXPECT_TRUE(fs::is_regular_file(staged));
  EXPECT_EQ(read_all(staged), "#!/bin/sh\necho hello\n");
}

TEST(upload_staging, the_name_is_not_predictable) {
  // Two uploads in a row never land on the same name - and a name that cannot
  // be predicted is one that cannot be planted ahead of time.
  scratch_dir dir;
  std::string error;

  const fs::path first = upload_staging::stage(dir.path, "a", error);
  const fs::path second = upload_staging::stage(dir.path, "b", error);

  ASSERT_FALSE(first.empty()) << error;
  ASSERT_FALSE(second.empty()) << error;
  EXPECT_NE(first, second);
  EXPECT_EQ(read_all(first), "a");
  EXPECT_EQ(read_all(second), "b");
}

TEST(upload_staging, binary_content_survives_intact) {
  scratch_dir dir;
  std::string error;
  std::string content("\x00\x01\r\n\xff\xfe binary \x00", 17);

  const fs::path staged = upload_staging::stage(dir.path, content, error);

  ASSERT_FALSE(staged.empty()) << error;
  EXPECT_EQ(read_all(staged), content);
}

TEST(upload_staging, an_empty_upload_produces_an_empty_file) {
  scratch_dir dir;
  std::string error;

  const fs::path staged = upload_staging::stage(dir.path, "", error);

  ASSERT_FALSE(staged.empty()) << error;
  EXPECT_TRUE(fs::is_regular_file(staged));
  EXPECT_EQ(fs::file_size(staged), 0u);
}

#ifndef WIN32
TEST(upload_staging, the_staged_file_is_readable_by_its_owner_only) {
  scratch_dir dir;
  std::string error;

  const fs::path staged = upload_staging::stage(dir.path, "secret", error);

  ASSERT_FALSE(staged.empty()) << error;
  const fs::perms mode = fs::status(staged).permissions() & fs::perms_mask;
  EXPECT_EQ(mode, fs::owner_read | fs::owner_write) << std::oct << static_cast<int>(mode);
}
#endif

TEST(upload_staging, a_missing_directory_is_an_error_not_a_file_somewhere_else) {
  scratch_dir dir;
  std::string error;

  const fs::path staged = upload_staging::stage(dir.path / "does-not-exist", "content", error);

  EXPECT_TRUE(staged.empty()) << staged;
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(fs::is_empty(dir.path)) << "nothing may be left behind";
}

TEST(upload_staging, create_exclusive_refuses_an_existing_file) {
  // The primitive stage() is built on: whatever is already at the path -
  // planted or leftover - is never reused or truncated.
  scratch_dir dir;
  const fs::path planted = dir.path / "planted";
  {
    std::ofstream out(planted.string().c_str());
    out << "attacker content";
  }
  std::string error;

  FILE *file = upload_staging::create_exclusive(planted, error);

  EXPECT_EQ(file, nullptr);
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(read_all(planted), "attacker content") << "the existing file must be left untouched";
}

TEST(upload_staging, create_exclusive_creates_a_fresh_file) {
  scratch_dir dir;
  const fs::path fresh = dir.path / "fresh";
  std::string error;

  FILE *file = upload_staging::create_exclusive(fresh, error);

  ASSERT_NE(file, nullptr) << error;
  EXPECT_GE(std::fputs("written", file), 0);
  EXPECT_EQ(std::fclose(file), 0);
  EXPECT_EQ(read_all(fresh), "written");
}

#ifndef WIN32
TEST(upload_staging, create_exclusive_does_not_follow_a_symlink) {
  // A symlink at the name pointing somewhere that does not exist yet: a
  // truncating open would follow it and create the target - the classic way a
  // temp-file write turns into a write anywhere the service can reach.
  scratch_dir dir;
  const fs::path target = dir.path / "target-that-must-not-appear";
  const fs::path link = dir.path / "link";
  fs::create_symlink(target, link);
  std::string error;

  FILE *file = upload_staging::create_exclusive(link, error);

  EXPECT_EQ(file, nullptr);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(fs::exists(target)) << "the link target must not have been created";
}
#endif
