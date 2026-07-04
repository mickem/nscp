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

#include "check_load.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

// Writes `content` to a temp file, runs check_load_from against it and returns
// the response.
PB::Common::ResultCode run_load(const std::string &content, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  const boost::filesystem::path p = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-load-%%%%-%%%%");
  {
    std::ofstream ofs(p.string().c_str());
    ofs << content << "\n";
  }
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_load");
  for (const std::string &a : args) request.add_arguments(a);
  load_check::check_load_from(request, &response, p.string());
  boost::filesystem::remove(p);
  return response.result();
}
}  // namespace

TEST(CheckLoad, ParsesLoadavg) {
  load_check::load_obj o;
  ASSERT_TRUE(load_check::parse_loadavg("0.52 0.58 0.59 1/834 12345", 4, false, o));
  EXPECT_DOUBLE_EQ(o.load1, 0.52);
  EXPECT_DOUBLE_EQ(o.load5, 0.58);
  EXPECT_DOUBLE_EQ(o.load15, 0.59);
  EXPECT_EQ(o.procs_running, 1);
  EXPECT_EQ(o.procs_total, 834);
  EXPECT_EQ(o.type, "total");
  EXPECT_DOUBLE_EQ(o.get_load(), 0.59);  // max of the three
}

TEST(CheckLoad, PercpuScalesByCpuCount) {
  load_check::load_obj o;
  ASSERT_TRUE(load_check::parse_loadavg("4.0 2.0 1.0 1/10 5", 4, true, o));
  EXPECT_DOUBLE_EQ(o.load1, 1.0);
  EXPECT_DOUBLE_EQ(o.load5, 0.5);
  EXPECT_DOUBLE_EQ(o.load15, 0.25);
  EXPECT_EQ(o.type, "scaled");
}

TEST(CheckLoad, PercpuSingleCpuIsUnscaled) {
  load_check::load_obj o;
  ASSERT_TRUE(load_check::parse_loadavg("4.0 2.0 1.0 1/10 5", 1, true, o));
  EXPECT_DOUBLE_EQ(o.load1, 4.0);
  EXPECT_EQ(o.type, "total");
}

TEST(CheckLoad, MalformedReturnsFalse) {
  load_check::load_obj o;
  EXPECT_FALSE(load_check::parse_loadavg("not-a-number", 1, false, o));
}

TEST(CheckLoad, NoDefaultThresholdIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_load("9.0 8.0 7.0 1/10 5", {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckLoad, UserThresholdTrips) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_load("5.0 4.0 3.0 2/100 5", {"warning=load1 > 4", "detail-syntax=${load1}/${load5}/${load15} ${type}"}, response),
            PB::Common::ResultCode::WARNING)
      << join_lines(response);
  EXPECT_NE(join_lines(response).find("5"), std::string::npos) << join_lines(response);
}

TEST(CheckLoad, LoadKeywordIsWorstWindow) {
  PB::Commands::QueryResponseMessage::Response response;
  // load = max(1,2,0.5) = 2 -> crit at load > 1.5
  EXPECT_EQ(run_load("1.0 2.0 0.5 1/10 5", {"critical=load > 1.5"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}
