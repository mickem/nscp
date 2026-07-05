// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_service.h"

#include <gtest/gtest.h>

using checks::check_svc_filter::compute_cpu_pct;
using checks::check_svc_filter::filter_obj;
using checks::check_svc_filter::parse_status_mem;
using checks::check_svc_filter::parse_stat_times;
using checks::check_svc_filter::parse_systemctl_show;

namespace {
std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

filter_obj make_svc(const std::string &name, const std::string &active, const std::string &sub, const std::string &unit_file, const std::string &preset) {
  filter_obj s;
  s.name = name;
  s.active = active;
  s.sub_state = sub;
  s.start_type = unit_file;
  s.preset = preset;
  s.state = filter_obj::map_state(active, sub, unit_file);
  return s;
}

PB::Common::ResultCode run(const std::vector<filter_obj> &svcs, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_service");
  for (const std::string &a : args) request.add_arguments(a);
  checks::check_service_evaluate(request, &response, svcs);
  return response.result();
}
}  // namespace

TEST(CheckService, MapsStateFromActiveSub) {
  EXPECT_EQ(filter_obj::map_state("active", "running", "enabled"), "running");
  EXPECT_EQ(filter_obj::map_state("active", "exited", "enabled"), "oneshot");
  EXPECT_EQ(filter_obj::map_state("activating", "start", "enabled"), "starting");
  EXPECT_EQ(filter_obj::map_state("inactive", "dead", "enabled"), "stopped");
  EXPECT_EQ(filter_obj::map_state("failed", "failed", "enabled"), "stopped");
  EXPECT_EQ(filter_obj::map_state("reloading", "reload", "enabled"), "unknown");
}

TEST(CheckService, StaticUnitFileOverridesState) {
  // A static unit maps to "static" regardless of its active/sub state.
  EXPECT_EQ(filter_obj::map_state("active", "running", "static"), "static");
  EXPECT_EQ(filter_obj::map_state("inactive", "dead", "static"), "static");
}

TEST(CheckService, StateToIntMatches) {
  EXPECT_EQ(filter_obj::state_to_int("stopped"), 1);
  EXPECT_EQ(filter_obj::state_to_int("starting"), 2);
  EXPECT_EQ(filter_obj::state_to_int("oneshot"), 3);
  EXPECT_EQ(filter_obj::state_to_int("running"), 4);
  EXPECT_EQ(filter_obj::state_to_int("static"), 5);
  EXPECT_EQ(filter_obj::state_to_int("unknown"), 0);
}

TEST(CheckService, ParseStateAcceptsStartedAlias) {
  EXPECT_EQ(filter_obj::parse_state("started"), filter_obj::state_running);
  EXPECT_EQ(filter_obj::parse_state("running"), filter_obj::state_running);
  EXPECT_EQ(filter_obj::parse_state("dead"), filter_obj::state_stopped);
}

// ---- /proc metric parsing --------------------------------------------------

TEST(CheckService, ParsesStatusMem) {
  long long rss = 0, vms = 0;
  ASSERT_TRUE(parse_status_mem("Name:\tbash\nVmSize:\t  12345 kB\nVmRSS:\t   2048 kB\n", rss, vms));
  EXPECT_EQ(rss, 2048LL * 1024);
  EXPECT_EQ(vms, 12345LL * 1024);
}

TEST(CheckService, ParsesStatTimesWithParensInComm) {
  unsigned long long utime = 0, stime = 0, starttime = 0;
  // comm "(a) (b)" contains spaces and parens; fields 14/15 utime/stime, 22 starttime.
  // pid (comm) state ppid pgrp sess tty tpgid flags min cmin maj cmaj utime stime ...
  const std::string stat =
      "1234 (weird (name)) S 1 1234 1234 0 -1 4194304 100 0 0 0 "
      "250 125 0 0 20 0 1 0 9876 8192000 500 " +
      std::string("18446744073709551615");
  ASSERT_TRUE(parse_stat_times(stat, utime, stime, starttime));
  EXPECT_EQ(utime, 250u);
  EXPECT_EQ(stime, 125u);
  EXPECT_EQ(starttime, 9876u);
}

TEST(CheckService, ComputesLifetimeCpuPercent) {
  // 300 ticks of CPU over a 30s window at 100Hz: proc_secs=3, elapsed=30 -> 10%.
  // starttime 1000 ticks = 10s in; uptime 40s -> elapsed 30s.
  EXPECT_DOUBLE_EQ(compute_cpu_pct(200, 100, 1000, 40.0, 100), 10.0);
  EXPECT_DOUBLE_EQ(compute_cpu_pct(200, 100, 1000, 10.0, 100), 0.0);  // non-positive elapsed
  EXPECT_DOUBLE_EQ(compute_cpu_pct(200, 100, 1000, 40.0, 0), 0.0);    // bad hz
}

// ---- systemctl show parsing ------------------------------------------------

TEST(CheckService, ParsesSystemctlShowBlocks) {
  const std::string output =
      "Id=docker.service\n"
      "Description=Docker Application Container Engine\n"
      "LoadState=loaded\n"
      "ActiveState=active\n"
      "SubState=running\n"
      "UnitFileState=enabled\n"
      "UnitFilePreset=disabled\n"
      "MainPID=999\n"
      "TasksCurrent=20\n"
      "\n"
      "Id=ssh.service\n"
      "ActiveState=inactive\n"
      "SubState=dead\n"
      "UnitFileState=enabled\n"
      "MainPID=0\n"
      "TasksCurrent=18446744073709551615\n";
  const std::vector<filter_obj> svcs = parse_systemctl_show(output);
  ASSERT_EQ(svcs.size(), 2u);

  EXPECT_EQ(svcs[0].name, "docker");
  EXPECT_EQ(svcs[0].active, "active");
  EXPECT_EQ(svcs[0].state, "running");
  EXPECT_EQ(svcs[0].preset, "disabled");
  EXPECT_EQ(svcs[0].pid, 999);
  EXPECT_EQ(svcs[0].tasks, 20);

  EXPECT_EQ(svcs[1].name, "ssh");
  EXPECT_EQ(svcs[1].active, "inactive");
  EXPECT_EQ(svcs[1].state, "stopped");
  EXPECT_EQ(svcs[1].pid, 0);
  EXPECT_EQ(svcs[1].tasks, 0);  // [not set] sentinel clamped
}

// ---- default filter / critical behavior ------------------

TEST(CheckService, DefaultIgnoresCleanlyStoppedServices) {
  // A disabled, stopped service is inactive -> excluded by the default filter
  // (active != inactive); with no other services the result is UNKNOWN/empty.
  PB::Commands::QueryResponseMessage::Response response;
  const auto svcs = std::vector<filter_obj>{make_svc("cups", "inactive", "dead", "disabled", "disabled")};
  EXPECT_EQ(run(svcs, {}, response), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

TEST(CheckService, DefaultCriticalOnFailedService) {
  PB::Commands::QueryResponseMessage::Response response;
  const auto svcs = std::vector<filter_obj>{make_svc("nginx", "failed", "failed", "enabled", "enabled")};
  EXPECT_EQ(run(svcs, {}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("nginx"), std::string::npos) << join_lines(response);
}

TEST(CheckService, RunningServiceIsOk) {
  PB::Commands::QueryResponseMessage::Response response;
  const auto svcs = std::vector<filter_obj>{make_svc("docker", "active", "running", "enabled", "enabled")};
  EXPECT_EQ(run(svcs, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckService, StaticAndOneshotAreOk) {
  PB::Commands::QueryResponseMessage::Response response;
  const auto svcs = std::vector<filter_obj>{
      make_svc("kmod-static-nodes", "active", "exited", "static", ""),
      make_svc("systemd-tmpfiles-setup", "active", "exited", "static", ""),
  };
  EXPECT_EQ(run(svcs, {}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckService, StateStartedAliasThresholdMatchesRunning) {
  // `state = 'started'` is aliased to running, so a running service passes.
  PB::Commands::QueryResponseMessage::Response response;
  const auto svcs = std::vector<filter_obj>{make_svc("docker", "active", "running", "enabled", "enabled")};
  EXPECT_EQ(run(svcs, {"critical=state != 'started'"}, response), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST(CheckService, RssThresholdTrips) {
  PB::Commands::QueryResponseMessage::Response response;
  filter_obj s = make_svc("docker", "active", "running", "enabled", "enabled");
  s.rss = 2LL * 1024 * 1024 * 1024;  // 2 GiB
  s.has_metrics = true;
  EXPECT_EQ(run({s}, {"critical=rss > 1073741824"}, response), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}
