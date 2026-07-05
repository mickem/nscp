// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_mount.hpp"

#include <gtest/gtest.h>

using check_mount_command::mount_point;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

mount_point mp(const std::string &mount, const std::string &device, const std::string &fstype, const std::string &options) {
  mount_point m;
  m.mount = mount;
  m.device = device;
  m.fstype = fstype;
  m.options = options;
  return m;
}

PB::Common::ResultCode run(const std::vector<mount_point> &mounts, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_mount");
  for (const std::string &a : args) request.add_arguments(a);
  check_mount_command::check_with(request, &response, mounts);
  return response.result();
}

const std::vector<mount_point> kMounts = {
    mp("/", "/dev/sda1", "ext4", "rw,relatime"),
    mp("/home", "/dev/sda2", "xfs", "rw,relatime,noexec"),
    mp("/proc", "proc", "proc", "rw,nosuid,nodev,noexec"),
    mp("/run", "tmpfs", "tmpfs", "rw,nosuid"),
};
}  // namespace

TEST(CheckMount, ExpectedMountIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {"mount=/", "fstype=ext4", "options=rw,relatime"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckMount, TrailingSlashIsStripped) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {"mount=/home/", "fstype=xfs"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckMount, MissingMountIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {"mount=/data"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("not mounted"), std::string::npos) << join_lines(response);
}

TEST(CheckMount, WrongFstypeWarns) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {"mount=/", "fstype=xfs"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("fstype"), std::string::npos) << join_lines(response);
}

TEST(CheckMount, MissingOptionWarns) {
  // / lacks 'noexec' -> reported as a missing option (issues != '' -> warning).
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {"mount=/", "options=rw,relatime,noexec"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("missing options: noexec"), std::string::npos) << join_lines(response);
}

TEST(CheckMount, ExceedingOptionWarns) {
  // / has 'relatime' beyond the expected set -> reported as exceeding.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {"mount=/", "options=rw"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("exceeding options: relatime"), std::string::npos) << join_lines(response);
}

TEST(CheckMount, ListAllSkipsInternalFilesystems) {
  // Without a specific mount, /proc and /run (pseudo/internal) are skipped, so
  // only the real / and /home mounts are considered (all OK).
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckMount, FstypeActsAsFilterWhenListingAll) {
  // fstype filters the listing (only ext4), and the ext4 mount is fine -> OK,
  // not critical about the xfs mount.
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run(kMounts, {"fstype=ext4"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckMount, EmptyMountTableForRequestedMountIsCritical) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run({}, {"mount=/"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}
