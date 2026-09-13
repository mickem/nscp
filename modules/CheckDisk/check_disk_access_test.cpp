// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The `file access` gate on the three disk checks which take a caller-supplied
// path: check_files, check_single_file and check_disk_write.
//
// Unlike check_logfile these never return file contents, but check_files walks
// whole directory trees and reports every name, size and timestamp, and the
// checksum keywords turn any readable file into a hash oracle. Where the caller
// chooses the argument - NRPE with `allow arguments`, or REST - that is a wide
// enough reach to be worth narrowing.
//
// The policy itself is covered by include/check/access_policy_test.cpp; what is
// tested here is that each command actually consults it, refuses before doing
// any work, and reports through the normal UNKNOWN path.

#include <check/path_access_policy.hpp>
#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <string>

#include "check_disk_write.hpp"
#include "check_files.hpp"
#include "check_single_file.hpp"
#include "test_support.hpp"

namespace {

namespace fs = boost::filesystem;
using check_disk_test_support::join_lines;
using ScratchDir = check_disk_test_support::ScratchDir;

/** A policy restricted to `dir`, in the mode named. */
check::access::path_policy restricted(const std::string &mode, const std::string &allowed) {
  check::access::path_policy policy("file", "files", "/settings/disk");
  policy.set_mode(mode);
  policy.set_allow_list(allowed);
  return policy;
}

PB::Commands::QueryRequestMessage::Request make_request(const std::string &command, const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

class DiskAccessTest : public ::testing::Test {
 protected:
  void SetUp() override {
    allowed_dir_ = (scratch_.path() / "logs").string();
    secret_dir_ = (scratch_.path() / "secret").string();
    fs::create_directories(fs::path(allowed_dir_) / "sub");
    fs::create_directories(secret_dir_);
    write(fs::path(allowed_dir_) / "app.log", "one\ntwo\n");
    write(fs::path(allowed_dir_) / "sub" / "deep.log", "deep\n");
    write(fs::path(secret_dir_) / "credentials", "hunter2\n");
  }
  static void write(const fs::path &p, const std::string &body) {
    std::ofstream f(p.string().c_str());
    f << body;
  }

  ScratchDir scratch_{"nscp-disk-access"};
  std::string allowed_dir_;
  std::string secret_dir_;
};

// ---------------------------------------------------------------------------
// check_files
// ---------------------------------------------------------------------------

TEST_F(DiskAccessTest, CheckFilesAnyModeScansAnyRoot) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_files", {"path=" + secret_dir_, "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, check_disk_test_support::unrestricted());

  EXPECT_NE(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST_F(DiskAccessTest, CheckFilesAllowsARootInsideTheList) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_files", {"path=" + allowed_dir_, "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_NE(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST_F(DiskAccessTest, CheckFilesRefusesARootOutsideTheList) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_files", {"path=" + secret_dir_, "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("Refusing file"), std::string::npos) << out;
  // The refusal must not carry the thing the caller was trying to enumerate.
  EXPECT_EQ(out.find("credentials"), std::string::npos) << out;
}

// The reason a path needs resolving before it is matched: as plain text this
// root reads as "under the allowed directory" and is not.
TEST_F(DiskAccessTest, CheckFilesRefusesATraversalOutOfTheAllowedRoot) {
  PB::Commands::QueryRequestMessage::Request request =
      make_request("check_files", {"path=" + (fs::path(allowed_dir_) / ".." / "secret").string(), "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing file"), std::string::npos) << join_lines(response);
}

// `paths=` is a comma separated alias for repeated `path=`; it is split before
// the gate runs, so its entries are held to the same list.
TEST_F(DiskAccessTest, CheckFilesGatesThePathsAlias) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_files", {"paths=" + allowed_dir_ + "," + secret_dir_, "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing file"), std::string::npos) << join_lines(response);
}

TEST_F(DiskAccessTest, CheckFilesRefusesEveryRootWhenOneIsNotAllowed) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_files", {"path=" + allowed_dir_, "path=" + secret_dir_, "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing file"), std::string::npos) << join_lines(response);
}

TEST_F(DiskAccessTest, CheckFilesPredefinedModeResolvesAName) {
  check::access::path_policy policy = restricted("predefined", "");
  policy.add_predefined("logs", allowed_dir_);

  PB::Commands::QueryRequestMessage::Request request = make_request("check_files", {"path=logs", "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, policy);

  EXPECT_NE(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST_F(DiskAccessTest, CheckFilesPredefinedModeRefusesARawPath) {
  check::access::path_policy policy = restricted("predefined", "");
  policy.add_predefined("logs", allowed_dir_);

  PB::Commands::QueryRequestMessage::Request request = make_request("check_files", {"path=" + allowed_dir_, "pattern=*"});
  PB::Commands::QueryResponseMessage::Response response;

  check_files_command::check(request, &response, policy);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("set to predefined"), std::string::npos) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_single_file
// ---------------------------------------------------------------------------

TEST_F(DiskAccessTest, CheckSingleFileAllowsAFileInsideTheList) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_single_file", {"file=" + (fs::path(allowed_dir_) / "app.log").string()});
  PB::Commands::QueryResponseMessage::Response response;

  check_single_file_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_NE(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST_F(DiskAccessTest, CheckSingleFileRefusesAFileOutsideTheList) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_single_file", {"file=" + (fs::path(secret_dir_) / "credentials").string()});
  PB::Commands::QueryResponseMessage::Response response;

  check_single_file_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("Refusing file"), std::string::npos) << out;
  // It must refuse rather than leak existence through the "File not found"
  // message the ungated path would produce for an unreadable file.
  EXPECT_EQ(out.find("File not found"), std::string::npos) << out;
}

// The `path=` alias reaches the same variable, so it is gated identically.
TEST_F(DiskAccessTest, CheckSingleFileGatesThePathAlias) {
  PB::Commands::QueryRequestMessage::Request request = make_request("check_single_file", {"path=" + (fs::path(secret_dir_) / "credentials").string()});
  PB::Commands::QueryResponseMessage::Response response;

  check_single_file_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing file"), std::string::npos) << join_lines(response);
}

// `ignore-missing` returns OK for a file which is not there; it must not turn a
// refusal into a silent OK, or the gate would be invisible.
TEST_F(DiskAccessTest, CheckSingleFileRefusalBeatsIgnoreMissing) {
  PB::Commands::QueryRequestMessage::Request request =
      make_request("check_single_file", {"file=" + (fs::path(secret_dir_) / "credentials").string(), "ignore-missing=true"});
  PB::Commands::QueryResponseMessage::Response response;

  check_single_file_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing file"), std::string::npos) << join_lines(response);
}

#ifndef WIN32
// stat_single_file follows a symbolic link, so resolving the path before the
// match is what stops a link inside an allowed directory reporting on its
// target.
TEST_F(DiskAccessTest, CheckSingleFileRefusesASymlinkLeadingOut) {
  boost::system::error_code ec;
  fs::create_symlink(fs::path(secret_dir_) / "credentials", fs::path(allowed_dir_) / "escape.log", ec);
  if (ec) GTEST_SKIP() << "cannot create symlinks here: " << ec.message();

  PB::Commands::QueryRequestMessage::Request request = make_request("check_single_file", {"file=" + (fs::path(allowed_dir_) / "escape.log").string()});
  PB::Commands::QueryResponseMessage::Response response;

  check_single_file_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing file"), std::string::npos) << join_lines(response);
}
#endif

// ---------------------------------------------------------------------------
// check_disk_write
// ---------------------------------------------------------------------------

TEST_F(DiskAccessTest, CheckDiskWriteAllowsAPathInsideTheList) {
  PB::Commands::QueryRequestMessage::Request request =
      make_request("check_disk_write", {"file=" + (fs::path(allowed_dir_) / "probe.tmp").string(), "size=1k"});
  PB::Commands::QueryResponseMessage::Response response;

  check_disk_write_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_NE(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST_F(DiskAccessTest, CheckDiskWriteRefusesAPathOutsideTheList) {
  const fs::path target = fs::path(secret_dir_) / "probe.tmp";
  PB::Commands::QueryRequestMessage::Request request = make_request("check_disk_write", {"file=" + target.string(), "size=1k"});
  PB::Commands::QueryResponseMessage::Response response;

  check_disk_write_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Refusing file"), std::string::npos) << join_lines(response);
  // Refused before anything was created.
  EXPECT_FALSE(fs::exists(target));
}

// The gate runs before the size is parsed, so a refusal is not masked by an
// argument error the caller could use to tell the two apart.
TEST_F(DiskAccessTest, CheckDiskWriteRefusesBeforeValidatingSize) {
  PB::Commands::QueryRequestMessage::Request request =
      make_request("check_disk_write", {"file=" + (fs::path(secret_dir_) / "probe.tmp").string(), "size=not-a-size"});
  PB::Commands::QueryResponseMessage::Response response;

  check_disk_write_command::check(request, &response, restricted("allowed", allowed_dir_));

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("Refusing file"), std::string::npos) << out;
  EXPECT_EQ(out.find("Invalid size"), std::string::npos) << out;
}

// ---------------------------------------------------------------------------
// a typo in the mode must not read as "no restriction"
// ---------------------------------------------------------------------------

TEST_F(DiskAccessTest, AnInvalidModeFailsClosedOnEveryCommand) {
  const check::access::path_policy policy = restricted("allwed", allowed_dir_);

  PB::Commands::QueryRequestMessage::Request files = make_request("check_files", {"path=" + allowed_dir_, "pattern=*"});
  PB::Commands::QueryResponseMessage::Response files_response;
  check_files_command::check(files, &files_response, policy);
  EXPECT_EQ(files_response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(files_response).find("expected any, allowed or predefined"), std::string::npos) << join_lines(files_response);

  PB::Commands::QueryRequestMessage::Request single =
      make_request("check_single_file", {"file=" + (fs::path(allowed_dir_) / "app.log").string()});
  PB::Commands::QueryResponseMessage::Response single_response;
  check_single_file_command::check(single, &single_response, policy);
  EXPECT_EQ(single_response.result(), PB::Common::ResultCode::UNKNOWN);

  PB::Commands::QueryRequestMessage::Request write_req =
      make_request("check_disk_write", {"file=" + (fs::path(allowed_dir_) / "probe.tmp").string(), "size=1k"});
  PB::Commands::QueryResponseMessage::Response write_response;
  check_disk_write_command::check(write_req, &write_response, policy);
  EXPECT_EQ(write_response.result(), PB::Common::ResultCode::UNKNOWN);
}

}  // namespace
