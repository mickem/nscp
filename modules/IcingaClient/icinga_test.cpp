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

#include "icinga.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(IcingaTest, MapExitStatusServicePassthrough) {
  EXPECT_EQ(icinga::map_exit_status(0, false), 0);
  EXPECT_EQ(icinga::map_exit_status(1, false), 1);
  EXPECT_EQ(icinga::map_exit_status(2, false), 2);
  EXPECT_EQ(icinga::map_exit_status(3, false), 3);
}

TEST(IcingaTest, MapExitStatusServiceClampOutOfRange) {
  EXPECT_EQ(icinga::map_exit_status(7, false), 3);
  EXPECT_EQ(icinga::map_exit_status(-1, false), 3);
}

TEST(IcingaTest, MapExitStatusHostClampToBinary) {
  // Icinga 2 (lib/icinga/apiactions.cpp) only accepts 0 or 1 for hosts.
  EXPECT_EQ(icinga::map_exit_status(0, true), 0);
  EXPECT_EQ(icinga::map_exit_status(1, true), 1);
  EXPECT_EQ(icinga::map_exit_status(2, true), 1);
  EXPECT_EQ(icinga::map_exit_status(3, true), 1);
  EXPECT_EQ(icinga::map_exit_status(99, true), 1);
}

TEST(IcingaTest, SplitPerfdataSimple) {
  const auto p = icinga::split_perfdata("a=1 b=2 c=3");
  ASSERT_EQ(p.size(), static_cast<std::size_t>(3));
  EXPECT_EQ(p[0], "a=1");
  EXPECT_EQ(p[1], "b=2");
  EXPECT_EQ(p[2], "c=3");
}

TEST(IcingaTest, SplitPerfdataQuotedLabel) {
  const auto p = icinga::split_perfdata("'C: drive'=10MB foo=bar");
  ASSERT_EQ(p.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(p[0], "'C: drive'=10MB");
  EXPECT_EQ(p[1], "foo=bar");
}

TEST(IcingaTest, SplitPerfdataEmpty) {
  EXPECT_TRUE(icinga::split_perfdata("").empty());
  EXPECT_TRUE(icinga::split_perfdata("   ").empty());
}

TEST(IcingaTest, BuildCheckResultBodyService) {
  const auto body = icinga::build_check_result_body(2, "CRIT", "load=0.9 mem=50%", "myhost", false);
  EXPECT_NE(body.find("\"exit_status\":2"), std::string::npos);
  EXPECT_NE(body.find("\"plugin_output\":\"CRIT\""), std::string::npos);
  EXPECT_NE(body.find("\"performance_data\":[\"load=0.9\",\"mem=50%\"]"), std::string::npos);
  EXPECT_NE(body.find("\"check_source\":\"myhost\""), std::string::npos);
}

TEST(IcingaTest, BuildCheckResultBodyHostClamp) {
  const auto body = icinga::build_check_result_body(2, "DOWN", "", "h", true);
  EXPECT_NE(body.find("\"exit_status\":1"), std::string::npos);
  // No performance_data should appear when perfdata is empty.
  EXPECT_EQ(body.find("performance_data"), std::string::npos);
}

TEST(IcingaTest, BuildHostCreateBodyDefaultTemplate) {
  const auto body = icinga::build_host_create_body("server1", "");
  EXPECT_NE(body.find("\"templates\":[\"generic-host\"]"), std::string::npos);
  EXPECT_NE(body.find("\"address\":\"server1\""), std::string::npos);
}

TEST(IcingaTest, BuildHostCreateBodyMultipleTemplates) {
  const auto body = icinga::build_host_create_body("server2", "myhost-tpl, base-host");
  EXPECT_NE(body.find("\"templates\":[\"myhost-tpl\",\"base-host\"]"), std::string::npos);
}

TEST(IcingaTest, BuildServiceCreateBodyDefaults) {
  const auto body = icinga::build_service_create_body("", "");
  EXPECT_NE(body.find("\"templates\":[\"generic-service\"]"), std::string::npos);
  EXPECT_NE(body.find("\"check_command\":\"dummy\""), std::string::npos);
}

TEST(IcingaTest, BuildServiceCreateBodyOverrides) {
  const auto body = icinga::build_service_create_body("svc-tpl", "passive");
  EXPECT_NE(body.find("\"templates\":[\"svc-tpl\"]"), std::string::npos);
  EXPECT_NE(body.find("\"check_command\":\"passive\""), std::string::npos);
}

TEST(IcingaTest, ParseCheckResultResponseSuccess) {
  const auto r = icinga::parse_check_result_response("{\"results\":[{\"code\":200,\"status\":\"Successfully processed\"}]}");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.message, "Successfully processed");
}

TEST(IcingaTest, ParseCheckResultResponseFailure) {
  const auto r = icinga::parse_check_result_response("{\"results\":[{\"code\":404,\"status\":\"No objects found.\"}]}");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.message, "No objects found.");
}

TEST(IcingaTest, ParseCheckResultResponseErrorEnvelope) {
  // 401/403 responses arrive as a plain envelope without `results`.
  const auto r = icinga::parse_check_result_response("{\"error\":401,\"status\":\"Unauthorized\"}");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.message, "Unauthorized");
}

TEST(IcingaTest, ParseCheckResultResponseEmpty) {
  const auto r = icinga::parse_check_result_response("");
  EXPECT_FALSE(r.ok);
  EXPECT_FALSE(r.message.empty());
}

TEST(IcingaTest, UrlEncode) {
  EXPECT_EQ(icinga::url_encode("simple"), "simple");
  EXPECT_EQ(icinga::url_encode("host!service"), "host%21service");
  EXPECT_EQ(icinga::url_encode("a b/c"), "a%20b%2Fc");
}
