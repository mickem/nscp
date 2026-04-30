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

#include "check_files.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <string>

#include "test_support.hpp"

namespace {

namespace fs = boost::filesystem;
using check_disk_test_support::join_lines;
using ScratchDir = check_disk_test_support::ScratchDir;

}  // namespace

// ============================================================================
// Argument validation
// ============================================================================

TEST(CheckFilesCommand, NoPathSpecifiedReturnsUnknown) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No path specified"), std::string::npos) << join_lines(response);
}

TEST(CheckFilesCommand, MissingPathIsReportedAsUnknown) {
  // Issue #613: a top-level path that does not exist must surface as UNKNOWN
  // with the offending path, not as a silent OK / "No files found".
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=Z:\\nscp_test_definitely_not_a_real_path_47b1f0e5");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Path was not found"), std::string::npos) << join_lines(response);
}

// ============================================================================
// Happy-path scans against a hermetic temp directory
// ============================================================================

TEST(CheckFilesCommand, EmptyDirectoryUsesDefaultEmptyStateUnknown) {
  // The default empty-state declared in check_files_command::check() is
  // "unknown". An empty directory therefore reports UNKNOWN (the legacy
  // CheckFiles wrapper overrides this to "ok" via empty-state=ok; that
  // override is verified separately below).
  ScratchDir dir;

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No files found"), std::string::npos) << join_lines(response);
}

TEST(CheckFilesCommand, EmptyStateOverrideMakesEmptyDirectoryOk) {
  const ScratchDir dir;

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("empty-state=ok");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
}

TEST(CheckFilesCommand, ScansFilesInDirectory) {
  const ScratchDir dir;
  dir.touch("a.txt");
  dir.touch("b.txt");
  dir.touch("c.txt");

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.txt");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  // Default detail-syntax includes "%(count) files"; with three matching
  // files the rendered top message must report all three.
  EXPECT_NE(join_lines(response).find("3"), std::string::npos) << join_lines(response);
}

TEST(CheckFilesCommand, MaxDepthZeroSkipsSubdirectories) {
  // Issue #730: max-depth=0 must mean "scan the top directory only".
  const ScratchDir dir;
  dir.touch("top.txt");
  fs::create_directory(dir.path() / "nested");
  std::ofstream((dir.path() / "nested" / "deep.txt").string()) << "x";

  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_files");
  request.add_arguments("path=" + dir.string());
  request.add_arguments("pattern=*.txt");
  request.add_arguments("max-depth=0");

  check_files_command::check(request, &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  // Only top.txt is reachable at depth 0; deep.txt must not be scanned.
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("1"), std::string::npos) << out;
  EXPECT_EQ(out.find("deep.txt"), std::string::npos) << out;
}
