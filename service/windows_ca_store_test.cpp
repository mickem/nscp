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

#ifdef WIN32

#include "windows_ca_store.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <list>
#include <sstream>
#include <string>

namespace {
std::string read_file(const std::string& path) {
  const std::ifstream is(path, std::ios::binary);
  std::stringstream ss;
  ss << is.rdbuf();
  return ss.str();
}

std::string make_temp_path() {
  const auto p = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-windows-ca-%%%%-%%%%-%%%%-%%%%.pem");
  return p.string();
}
}  // namespace

TEST(WindowsCaStore, ExportProducesNonEmptyBundle) {
  const std::string path = make_temp_path();
  std::list<std::string> errors;

  const unsigned int n = nsclient::windows_ca::export_root_store(path, errors);

  // Every Windows install ships at least a handful of trusted root CAs.
  EXPECT_GT(n, 0u);
  EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());

  ASSERT_TRUE(boost::filesystem::exists(path));
  const std::string body = read_file(path);
  EXPECT_FALSE(body.empty());

  boost::filesystem::remove(path);
}

TEST(WindowsCaStore, ExportWritesPemFraming) {
  const std::string path = make_temp_path();
  std::list<std::string> errors;

  ASSERT_GT(nsclient::windows_ca::export_root_store(path, errors), 0u);
  const std::string body = read_file(path);

  // Every cert is wrapped in BEGIN/END CERTIFICATE markers.
  EXPECT_NE(body.find("-----BEGIN CERTIFICATE-----"), std::string::npos);
  EXPECT_NE(body.find("-----END CERTIFICATE-----"), std::string::npos);

  boost::filesystem::remove(path);
}

TEST(WindowsCaStore, ExportFailsCleanlyOnUnwritablePath) {
  // A path that's almost certainly not writable; we expect an error message
  // rather than a crash and zero certs written.
  const std::string path = "Z:/this-volume-does-not-exist/nscp-test.pem";
  std::list<std::string> errors;

  const unsigned int n = nsclient::windows_ca::export_root_store(path, errors);

  EXPECT_EQ(n, 0u);
  EXPECT_FALSE(errors.empty());
}

#endif  // WIN32
