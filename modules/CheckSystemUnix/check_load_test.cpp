// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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

// --- a reader without the /proc/loadavg process counts (macOS) ---------------

TEST(CheckLoad, UnknownProcessCountsRenderAsUnknown) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_load");
  request.add_arguments("detail-syntax=run=${procs_running} total=${procs_total}");
  request.add_arguments("warning=procs_running > 0");
  PB::Commands::QueryResponseMessage::Response response;
  load_check::check_load_with(request, &response, [](const bool percpu, load_check::load_obj &out, std::string &) {
    out.load1 = 8.0;
    out.load5 = 4.0;
    out.load15 = 2.0;
    out.procs_total = 1500;
    out.has_procs_total = true;
    load_check::apply_percpu(out, 4, percpu);
    return true;
  });
  // A missing count never satisfies a threshold.
  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("run=unknown total=1500"), std::string::npos) << join_lines(response);
}

TEST(CheckLoad, AReadErrorIsUnknownWithTheReason) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_load");
  PB::Commands::QueryResponseMessage::Response response;
  load_check::check_load_with(request, &response, [](const bool, load_check::load_obj &, std::string &error) {
    error = "getloadavg failed";
    return false;
  });
  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("getloadavg failed"), std::string::npos) << join_lines(response);
}

TEST(CheckLoad, PercpuScalesAReaderSample) {
  load_check::load_obj o;
  o.load1 = 8.0;
  o.load5 = 4.0;
  o.load15 = 2.0;
  load_check::apply_percpu(o, 4, true);
  EXPECT_EQ(o.type, "scaled");
  EXPECT_DOUBLE_EQ(o.load1, 2.0);
  load_check::load_obj one;
  one.load1 = 3.0;
  load_check::apply_percpu(one, 1, true);
  EXPECT_EQ(one.type, "total");
  EXPECT_DOUBLE_EQ(one.load1, 3.0);
}
