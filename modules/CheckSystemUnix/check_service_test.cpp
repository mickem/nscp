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

// ---- field getters and show ------------------------------------------------

TEST(CheckService, FieldGettersExposeRawValues) {
  filter_obj s = make_svc("docker", "active", "running", "enabled", "enabled");
  s.desc = "Docker Application Container Engine";
  s.load_state = "loaded";
  s.pid = 999;
  s.rss = 1024;
  s.vms = 4096;
  s.cpu = 1.5;
  s.tasks = 20;
  s.created = 1700000000;
  s.age = 3600;

  EXPECT_EQ(s.get_name(), "docker");
  EXPECT_EQ(s.get_desc(), "Docker Application Container Engine");
  EXPECT_EQ(s.get_active(), "active");
  EXPECT_EQ(s.get_state_s(), "running");
  EXPECT_EQ(s.get_sub_state(), "running");
  EXPECT_EQ(s.get_load_state(), "loaded");
  EXPECT_EQ(s.get_start_type_s(), "enabled");
  EXPECT_EQ(s.get_preset(), "enabled");
  EXPECT_EQ(s.get_pid(), 999);
  EXPECT_EQ(s.get_rss(), 1024);
  EXPECT_EQ(s.get_vms(), 4096);
  EXPECT_DOUBLE_EQ(s.get_cpu(), 1.5);
  EXPECT_EQ(s.get_tasks(), 20);
  EXPECT_EQ(s.get_created(), 1700000000);
  EXPECT_EQ(s.get_age(), 3600);
  EXPECT_EQ(s.show(), "docker:running");
  EXPECT_EQ(s.get_state_i(), filter_obj::state_running);
}

TEST(CheckService, ParseStateStaticAndUnknown) {
  EXPECT_EQ(filter_obj::parse_state("static"), filter_obj::state_static);
  EXPECT_EQ(filter_obj::parse_state("oneshot"), filter_obj::state_oneshot);
  EXPECT_EQ(filter_obj::parse_state("exited"), filter_obj::state_oneshot);
  EXPECT_EQ(filter_obj::parse_state("bogus"), filter_obj::state_unknown);
}

// ---- active-state booleans ---------------------------------------------------

TEST(CheckService, ActiveStateBooleans) {
  const filter_obj running = make_svc("a", "active", "running", "enabled", "");
  EXPECT_TRUE(running.is_running());
  EXPECT_TRUE(running.is_started());
  EXPECT_FALSE(running.is_stopped());
  EXPECT_FALSE(running.is_failed());
  EXPECT_EQ(running.get_started(), 1);
  EXPECT_EQ(running.get_stopped(), 0);

  const filter_obj oneshot = make_svc("b", "active", "exited", "enabled", "");
  EXPECT_FALSE(oneshot.is_running());  // running means sub-state running, not just active
  EXPECT_TRUE(oneshot.is_started());

  const filter_obj stopped = make_svc("c", "inactive", "dead", "disabled", "");
  EXPECT_FALSE(stopped.is_running());
  EXPECT_FALSE(stopped.is_started());
  EXPECT_TRUE(stopped.is_stopped());
  EXPECT_FALSE(stopped.is_failed());
  EXPECT_EQ(stopped.get_started(), 0);
  EXPECT_EQ(stopped.get_stopped(), 1);

  const filter_obj failed = make_svc("d", "failed", "failed", "enabled", "");
  EXPECT_TRUE(failed.is_stopped());  // failed counts as stopped
  EXPECT_TRUE(failed.is_failed());
}

// ---- start-type helpers ------------------------------------------------------

TEST(CheckService, StartTypeBooleansIncludeRuntimeVariants) {
  filter_obj s = make_svc("a", "active", "running", "enabled", "");
  EXPECT_TRUE(s.is_enabled());
  s.start_type = "enabled-runtime";
  EXPECT_TRUE(s.is_enabled());
  s.start_type = "disabled";
  EXPECT_TRUE(s.is_disabled());
  EXPECT_FALSE(s.is_enabled());
  s.start_type = "static";
  EXPECT_TRUE(s.is_static());
  s.start_type = "masked";
  EXPECT_TRUE(s.is_masked());
  s.start_type = "masked-runtime";
  EXPECT_TRUE(s.is_masked());
}

TEST(CheckService, StartTypeToIntCoversAllVariants) {
  filter_obj s = make_svc("a", "active", "running", "enabled", "");
  EXPECT_EQ(s.get_start_type_i(), filter_obj::start_type_enabled);
  s.start_type = "disabled";
  EXPECT_EQ(s.get_start_type_i(), filter_obj::start_type_disabled);
  s.start_type = "static";
  EXPECT_EQ(s.get_start_type_i(), filter_obj::start_type_static);
  s.start_type = "masked";
  EXPECT_EQ(s.get_start_type_i(), filter_obj::start_type_masked);
  s.start_type = "generated";
  EXPECT_EQ(s.get_start_type_i(), filter_obj::start_type_unknown);
}

TEST(CheckService, ParseStartTypeAcceptsWindowsStyleAliases) {
  EXPECT_EQ(filter_obj::parse_start_type("enabled"), filter_obj::start_type_enabled);
  EXPECT_EQ(filter_obj::parse_start_type("auto"), filter_obj::start_type_enabled);
  EXPECT_EQ(filter_obj::parse_start_type("disabled"), filter_obj::start_type_disabled);
  EXPECT_EQ(filter_obj::parse_start_type("manual"), filter_obj::start_type_disabled);
  EXPECT_EQ(filter_obj::parse_start_type("static"), filter_obj::start_type_static);
  EXPECT_EQ(filter_obj::parse_start_type("masked"), filter_obj::start_type_masked);
  EXPECT_EQ(filter_obj::parse_start_type("bogus"), filter_obj::start_type_unknown);
}

// ---- expectation helpers (state_is_ok / state_is_perfect) --------------------

TEST(CheckService, StateIsOkExpectations) {
  // masked: only OK when actually stopped.
  EXPECT_TRUE(make_svc("a", "inactive", "dead", "masked", "").state_is_ok());
  EXPECT_FALSE(make_svc("a", "active", "running", "masked", "").state_is_ok());
  // disabled: any state is acceptable.
  EXPECT_TRUE(make_svc("b", "active", "running", "disabled", "").state_is_ok());
  EXPECT_TRUE(make_svc("b", "inactive", "dead", "disabled", "").state_is_ok());
  // enabled: must at least be started (running or oneshot both count).
  EXPECT_TRUE(make_svc("c", "active", "running", "enabled", "").state_is_ok());
  EXPECT_TRUE(make_svc("c", "active", "exited", "enabled", "").state_is_ok());
  EXPECT_FALSE(make_svc("c", "inactive", "dead", "enabled", "").state_is_ok());
  EXPECT_FALSE(make_svc("c", "failed", "failed", "enabled", "").state_is_ok());
  // static / unknown types: always OK.
  EXPECT_TRUE(make_svc("d", "inactive", "dead", "static", "").state_is_ok());
  EXPECT_TRUE(make_svc("e", "inactive", "dead", "generated", "").state_is_ok());
}

TEST(CheckService, StateIsPerfectIsStricterThanOk) {
  // disabled: perfect only when stopped (ok allows running too).
  EXPECT_TRUE(make_svc("a", "inactive", "dead", "disabled", "").state_is_perfect());
  EXPECT_FALSE(make_svc("a", "active", "running", "disabled", "").state_is_perfect());
  // enabled: perfect requires sub-state running — oneshot is merely ok.
  EXPECT_TRUE(make_svc("b", "active", "running", "enabled", "").state_is_perfect());
  EXPECT_FALSE(make_svc("b", "active", "exited", "enabled", "").state_is_perfect());
  EXPECT_FALSE(make_svc("b", "inactive", "dead", "enabled", "").state_is_perfect());
  // masked: perfect when stopped only.
  EXPECT_TRUE(make_svc("c", "inactive", "dead", "masked", "").state_is_perfect());
  EXPECT_FALSE(make_svc("c", "active", "running", "masked", "").state_is_perfect());
  // static / unknown types: always perfect.
  EXPECT_TRUE(make_svc("d", "active", "running", "static", "").state_is_perfect());
  EXPECT_TRUE(make_svc("e", "inactive", "dead", "generated", "").state_is_perfect());
}

// ---- keyword plumbing through the real filter --------------------------------

TEST(CheckService, StartTypeKeywordFiltersThroughEvaluate) {
  // Drive the same getters through the production filter registry: only the
  // enabled-but-stopped service should trip the threshold.
  PB::Commands::QueryResponseMessage::Response response;
  filter_obj bad = make_svc("crashed", "inactive", "dead", "enabled", "enabled");
  filter_obj good = make_svc("docker", "active", "running", "enabled", "enabled");
  const auto rc = run({bad, good}, {"filter=start_type = 'enabled'", "critical=state = 'stopped'"}, response);
  EXPECT_EQ(rc, PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("crashed"), std::string::npos) << join_lines(response);
}
