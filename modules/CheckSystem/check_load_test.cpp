// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_load.hpp"

#include <gtest/gtest.h>

#include <cmath>

using load_check::load_avg_state;
using load_check::load_obj;
using load_check::make_load_obj;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

bool has_perf(const PB::Commands::QueryResponseMessage::Response &r, const std::string &alias_part) {
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) {
      if (r.lines(i).perf(j).alias().find(alias_part) != std::string::npos) return true;
    }
  }
  return false;
}

load_avg_state state_after(const int ticks, const double queue, const double busy_cores) {
  load_avg_state s;
  for (int i = 0; i < ticks; ++i) s.update(queue, busy_cores, 1.0);
  s.cores = 4;
  s.procs_total = 1234;
  return s;
}

PB::Common::ResultCode run_check(const load_avg_state &state, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_load");
  for (const std::string &a : args) request.add_arguments(a);
  load_check::check_load_from(request, &response, state);
  return response.result();
}

}  // namespace

// --- EMA state ----------------------------------------------------------------

TEST(CheckLoadAvg, FirstSampleSeedsTheAverages) {
  load_avg_state s;
  s.update(2.0, 3.0, 1.0);
  EXPECT_DOUBLE_EQ(s.load1, 5.0);
  EXPECT_DOUBLE_EQ(s.load5, 5.0);
  EXPECT_DOUBLE_EQ(s.load15, 5.0);
  EXPECT_DOUBLE_EQ(s.queue1, 2.0);
  EXPECT_DOUBLE_EQ(s.last_instant, 5.0);
  EXPECT_EQ(s.samples, 1);
}

TEST(CheckLoadAvg, ConstantInputStaysConstant) {
  const load_avg_state s = state_after(600, 1.0, 3.0);
  EXPECT_NEAR(s.load1, 4.0, 1e-9);
  EXPECT_NEAR(s.load5, 4.0, 1e-9);
  EXPECT_NEAR(s.load15, 4.0, 1e-9);
  EXPECT_NEAR(s.queue1, 1.0, 1e-9);
  EXPECT_EQ(s.samples, 600);
}

TEST(CheckLoadAvg, ShortWindowReactsFasterOnFallingLoad) {
  load_avg_state s;
  s.update(0.0, 8.0, 1.0);                           // seed at 8
  for (int i = 0; i < 60; ++i) s.update(0, 0, 1.0);  // one idle minute
  // After 60 s of idle: load1 ~ 8*e^-1 ~ 2.94, load15 ~ 8*e^(-60/900) ~ 7.48.
  EXPECT_LT(s.load1, s.load5);
  EXPECT_LT(s.load5, s.load15);
  EXPECT_NEAR(s.load1, 8.0 * std::exp(-1.0), 0.1);
  EXPECT_NEAR(s.load15, 8.0 * std::exp(-60.0 / 900.0), 0.1);
}

TEST(CheckLoadAvg, ShortWindowReactsFasterOnRisingLoad) {
  load_avg_state s;
  s.update(0.0, 0.0, 1.0);                               // seed idle
  for (int i = 0; i < 60; ++i) s.update(2.0, 6.0, 1.0);  // one busy minute at 8
  EXPECT_GT(s.load1, s.load5);
  EXPECT_GT(s.load5, s.load15);
}

TEST(CheckLoadAvg, DecayFollowsTheMeasuredInterval) {
  // The collector runs long whenever a tick overruns, so one 60 s fold must
  // decay exactly as far as sixty 1 s folds, not by a single second.
  load_avg_state fine;
  fine.update(0.0, 8.0, 1.0);
  for (int i = 0; i < 60; ++i) fine.update(0.0, 0.0, 1.0);

  load_avg_state coarse;
  coarse.update(0.0, 8.0, 1.0);
  coarse.update(0.0, 0.0, 60.0);

  EXPECT_NEAR(coarse.load1, fine.load1, 1e-9);
  EXPECT_NEAR(coarse.load5, fine.load5, 1e-9);
  EXPECT_NEAR(coarse.load15, fine.load15, 1e-9);
  EXPECT_NEAR(coarse.load1, 8.0 * std::exp(-1.0), 1e-9);
}

TEST(CheckLoadAvg, NonPositiveIntervalDoesNotFreezeOrWipeTheAverages) {
  // A backwards or stalled clock must not make the averages stick or collapse.
  load_avg_state s;
  s.update(0.0, 8.0, 1.0);
  s.update(0.0, 0.0, -5.0);
  EXPECT_LT(s.load1, 8.0);
  EXPECT_GT(s.load1, 7.9);
  s.update(0.0, 0.0, 1e9);  // clamped to the 15-minute window, not infinite decay
  EXPECT_GE(s.load15, 0.0);
  EXPECT_LT(s.load15, 8.0);
}

// --- row building ---------------------------------------------------------------

TEST(CheckLoadAvg, PercpuScalesLoadsButNotQueue) {
  const load_avg_state s = state_after(600, 2.0, 6.0);  // load 8 on 4 cores
  const load_obj scaled = make_load_obj(s, true);
  EXPECT_EQ(scaled.type, "scaled");
  EXPECT_NEAR(scaled.load1, 2.0, 1e-6);
  EXPECT_NEAR(scaled.queue, 2.0, 1e-6);  // absolute thread count, never divided
  const load_obj total = make_load_obj(s, false);
  EXPECT_EQ(total.type, "total");
  EXPECT_NEAR(total.load1, 8.0, 1e-6);
  EXPECT_EQ(total.procs_running, 8);
  EXPECT_EQ(total.procs_total, 1234);
}

// --- check rendering / thresholds -----------------------------------------------

TEST(CheckLoad, DefaultIsOkAndEmitsLoadPerf) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(state_after(600, 1.0, 3.0), {}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("total load average: 4"), std::string::npos) << join_lines(response);
  EXPECT_TRUE(has_perf(response, "load1")) << join_lines(response);
  EXPECT_TRUE(has_perf(response, "load5"));
  EXPECT_TRUE(has_perf(response, "load15"));
}

TEST(CheckLoad, WarningThresholdTrips) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(state_after(600, 2.0, 6.0), {"warn=load1 > 5"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
}

TEST(CheckLoad, PercpuReportsScaledType) {
  PB::Commands::QueryResponseMessage::Response response;
  // percpu=true as a valued token (the REST transport form).
  EXPECT_EQ(run_check(state_after(600, 2.0, 6.0), {"percpu=true", "warn=load1 > 5"}, response), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("scaled load average: 2"), std::string::npos) << join_lines(response);
}

TEST(CheckLoad, QueueAndSamplesKeywordsAreThresholdable) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check(state_after(600, 6.0, 2.0), {"crit=queue > 4 and samples > 60"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}
