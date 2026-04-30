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

#include "check_single_file.hpp"

#include <gtest/gtest.h>

#include "test_support.hpp"

namespace {

using check_disk_test_support::join_lines;
using ScratchDir = check_disk_test_support::ScratchDir;

}  // namespace

// ============================================================================
// Argument validation
// ============================================================================

TEST(CheckSingleFileCommand, NoFileSpecifiedReturnsUnknown) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_single_file");

  check_single_file_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No file specified"), std::string::npos) << join_lines(response);
}

TEST(CheckSingleFileCommand, MissingFileReturnsUnknown) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_single_file");
  request.add_arguments("file=Z:\\nscp_test_definitely_not_a_real_path_47b1f0e5\\foo.dat");

  check_single_file_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("File not found"), std::string::npos) << out;
  EXPECT_NE(out.find("foo.dat"), std::string::npos) << out;
}

// ============================================================================
// Happy-path: stat a real file via both `file=` and the `path=` alias.
// ============================================================================

TEST(CheckSingleFileCommand, ExistingFileReturnsOk) {
  const ScratchDir dir;
  const std::string file_path = dir.make_file("hello.txt", "hello world");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_single_file");
  request.add_arguments("file=" + file_path);

  check_single_file_command::check(request, &response);

  // No thresholds and a single matching file → default-empty-state ("ok")
  // path; the file's name must appear in the rendered detail line.
  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  EXPECT_NE(join_lines(response).find("hello.txt"), std::string::npos) << join_lines(response);
}

TEST(CheckSingleFileCommand, PathAliasIsAcceptedForFile) {
  const ScratchDir dir;
  const std::string file_path = dir.make_file("aliased.log", "abc");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_single_file");
  request.add_arguments("path=" + file_path);

  check_single_file_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  EXPECT_NE(join_lines(response).find("aliased.log"), std::string::npos) << join_lines(response);
}

TEST(CheckSingleFileCommand, SizeThresholdTriggersCritical) {
  // 16 bytes of contents must trip a "size > 8 bytes" critical threshold.
  const ScratchDir dir;
  const std::string file_path = dir.make_file("big.dat", "0123456789abcdef");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_single_file");
  request.add_arguments("file=" + file_path);
  request.add_arguments("crit=size > 8B");

  check_single_file_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL);
}
