// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "diagnostics_systemd.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace systemd_diagnostics;

namespace {
bool has(const std::vector<std::string> &v, const std::string &s) { return std::find(v.begin(), v.end(), s) != v.end(); }

// Captured from `systemctl show --property=...` on Ubuntu 24.04 (systemd 255).
const char *const show_output =
    "MainPID=197706\n"
    "Result=success\n"
    "NRestarts=40\n"
    "ExecMainStartTimestampMonotonic=74565064317\n"
    "ExecMainCode=0\n"
    "ExecMainStatus=0\n"
    "ExecStart={ path=/usr/libexec/wsl-pro-service ; argv[]=/usr/libexec/wsl-pro-service ; ignore_errors=no ; start_time=[Sun 2026-11-15 09:52:05 CET] ; "
    "stop_time=[n/a] ; pid=197706 ; code=(null) ; status=0/0 }\n"
    "User=\n"
    "Id=wsl-pro.service\n"
    "LoadState=loaded\n"
    "ActiveState=active\n"
    "SubState=running\n"
    "\n"
    "MainPID=230\n"
    "Result=success\n"
    "NRestarts=0\n"
    "ExecMainStartTimestampMonotonic=1950413\n"
    "ExecMainCode=0\n"
    "ExecMainStatus=0\n"
    "ExecStart={ path=/usr/sbin/cron ; argv[]=/usr/sbin/cron -f -P $EXTRA_OPTS ; ignore_errors=no ; start_time=[n/a] ; stop_time=[n/a] ; pid=0 ; "
    "code=(null) ; status=0/0 }\n"
    "User=\n"
    "Id=cron.service\n"
    "LoadState=loaded\n"
    "ActiveState=active\n"
    "SubState=running\n"
    "\n"
    "MainPID=0\n"
    "Result=exit-code\n"
    "NRestarts=7\n"
    "ExecMainStartTimestampMonotonic=74000000000\n"
    "ExecMainCode=1\n"
    "ExecMainStatus=1\n"
    "ExecStart={ path=/opt/acme/bin/acme-sync ; argv[]=/opt/acme/bin/acme-sync --token=hunter2 ; ignore_errors=no ; start_time=[n/a] ; stop_time=[n/a] ; "
    "pid=4242 ; code=exited ; status=1 }\n"
    "User=acme\n"
    "Id=acme-sync.service\n"
    "LoadState=loaded\n"
    "ActiveState=activating\n"
    "SubState=auto-restart\n";
}  // namespace

TEST(systemd_diagnostics, list_units_and_candidates) {
  const std::vector<unit_listing> units = parse_list_units(
      "apparmor.service                         loaded    inactive dead    Load AppArmor profiles\n"
      "acme-sync.service                        loaded    activating auto-restart Acme catalogue sync\n"
      "broken.service                           loaded    failed   failed  Broken\n"
      "cron.service                             loaded    active   running Regular background program processing daemon\n"
      "gone.service                             not-found inactive dead    gone.service\n"
      "setup.service                            loaded    active   exited  One-shot setup\n"
      "\n"
      "garbage\n");
  ASSERT_EQ(units.size(), 6u);
  std::vector<std::string> picked;
  for (const unit_listing &u : units)
    if (is_candidate(u)) picked.push_back(u.id);
  EXPECT_EQ(picked, (std::vector<std::string>{"acme-sync.service", "broken.service", "cron.service"}));

  // A sweep reads the running units too; in between only the troubled ones...
  EXPECT_EQ(select_units(units, true, {}), (std::vector<std::string>{"acme-sync.service", "broken.service", "cron.service"}));
  EXPECT_EQ(select_units(units, false, {}), (std::vector<std::string>{"acme-sync.service", "broken.service"}));
  // ...and the running ones the last sweep found restarted. A name in that
  // set that is no longer running, or no longer there, adds nothing.
  EXPECT_EQ(select_units(units, false, {"cron.service", "setup.service", "left.service"}),
            (std::vector<std::string>{"acme-sync.service", "broken.service", "cron.service"}));
  EXPECT_TRUE(is_running(units[3]));
  EXPECT_FALSE(is_running(units[1]));
  EXPECT_FALSE(is_running(units[5]));
}

TEST(systemd_diagnostics, exec_path_is_the_path_not_the_argv) {
  EXPECT_EQ(exec_path("{ path=/opt/acme/bin/acme-sync ; argv[]=/opt/acme/bin/acme-sync --token=hunter2 ; ignore_errors=no }"), "/opt/acme/bin/acme-sync");
  EXPECT_EQ(exec_path(""), "");
  EXPECT_EQ(exec_path("{ argv[]=/bin/true }"), "");
}

TEST(systemd_diagnostics, problem_rows_from_show) {
  const auto units = parse_show(show_output);
  ASSERT_EQ(units.size(), 3u);
  // Monotonic now: 74620.53 s since boot.
  const std::int64_t mono = 74620530000;
  const std::int64_t wall = 1794728000;

  service_tables::service_row row;
  ASSERT_TRUE(problem_row(units[0], wall, mono, row));
  EXPECT_EQ(row.name, "wsl-pro");
  EXPECT_EQ(row.state, "active (running)");
  EXPECT_FALSE(row.has_exit_code);
  EXPECT_EQ(row.binary_path, "/usr/libexec/wsl-pro-service");
  EXPECT_EQ(row.account, "root");
  EXPECT_EQ(row.started, wall - 55);
  EXPECT_TRUE(row.has_restart_count);
  EXPECT_EQ(row.restart_count, 40);

  // Running, never restarted, last run fine: not a problem.
  EXPECT_FALSE(problem_row(units[1], wall, mono, row));

  ASSERT_TRUE(problem_row(units[2], wall, mono, row));
  EXPECT_EQ(row.name, "acme-sync");
  EXPECT_EQ(row.state, "activating (auto-restart)");
  EXPECT_TRUE(row.has_exit_code);
  EXPECT_EQ(row.exit_code, 1);
  // Never the argv, which here carries a token.
  EXPECT_EQ(row.binary_path, "/opt/acme/bin/acme-sync");
  EXPECT_EQ(row.account, "acme");
  // No main process: no start time.
  EXPECT_EQ(row.started, 0);
  EXPECT_EQ(row.restart_count, 7);
}

TEST(systemd_diagnostics, failed_and_unsuccessful_units_are_problems) {
  service_tables::service_row row;
  EXPECT_TRUE(problem_row({{"Id", "broken.service"}, {"ActiveState", "failed"}, {"SubState", "failed"}, {"Result", "exit-code"}}, 0, 0, row));
  EXPECT_EQ(row.state, "failed");
  EXPECT_FALSE(row.has_restart_count);
  EXPECT_TRUE(problem_row({{"Id", "x.service"}, {"ActiveState", "inactive"}, {"SubState", "dead"}, {"Result", "timeout"}}, 0, 0, row));
  EXPECT_FALSE(problem_row({{"Id", "y.service"}, {"ActiveState", "inactive"}, {"SubState", "dead"}, {"Result", "success"}}, 0, 0, row));
  EXPECT_FALSE(problem_row({{"Id", "z.service"}, {"LoadState", "not-found"}, {"ActiveState", "failed"}}, 0, 0, row));
}

TEST(systemd_diagnostics, journal_args) {
  const std::vector<std::string> first = journal_args(feed::errors, "", 1700000000, 50);
  EXPECT_TRUE(has(first, "--since=@1700000000"));
  EXPECT_TRUE(has(first, "--priority=0..3"));
  EXPECT_TRUE(has(first, "--output=json"));
  EXPECT_TRUE(has(first, "--lines=50"));
  const std::vector<std::string> next = journal_args(feed::service_events, "s=abc;i=1", 1700000000, 50);
  EXPECT_TRUE(has(next, "--after-cursor=s=abc;i=1"));
  EXPECT_FALSE(has(next, "--since=@1700000000"));
  EXPECT_TRUE(has(next, "_PID=1"));
  EXPECT_TRUE(has(next, std::string("MESSAGE_ID=") + message_unit_restart));
  EXPECT_FALSE(has(next, "--priority=0..3"));
}

TEST(systemd_diagnostics, journal_service_events) {
  // Trimmed from `journalctl -o json` on systemd 255.
  const std::string out =
      R"({"__CURSOR":"s=1;i=a","__REALTIME_TIMESTAMP":"1794727133221820","_PID":"1","MESSAGE_ID":"5eb03494b6584870a536b337290809b3","UNIT":"wsl-pro.service","MESSAGE":"wsl-pro.service: Scheduled restart job, restart counter is at 37.","PRIORITY":"6"})"
      "\n"
      R"({"__CURSOR":"s=1;i=b","__REALTIME_TIMESTAMP":"1794727140000000","_PID":"1","MESSAGE_ID":"98e322203f7a4ed290d09fe03c09fe15","UNIT":"cron.service","EXIT_CODE":"exited","EXIT_STATUS":"0","MESSAGE":"cron.service: Main process exited, code=exited, status=0/SUCCESS"})"
      "\n"
      R"({"__CURSOR":"s=1;i=c","__REALTIME_TIMESTAMP":"1794727150000000","_PID":"1","MESSAGE_ID":"98e322203f7a4ed290d09fe03c09fe15","UNIT":"acme-sync.service","EXIT_CODE":"exited","EXIT_STATUS":"1","MESSAGE":"acme-sync.service: Main process exited, code=exited, status=1/FAILURE"})"
      "\n"
      "not json\n";
  const journal_read read = parse_journal(feed::service_events, out);
  // The clean exit is no event, but the cursor moves past it.
  ASSERT_EQ(read.rows.size(), 2u);
  EXPECT_EQ(read.cursor, "s=1;i=c");
  EXPECT_EQ(read.rows[0].service, "wsl-pro");
  EXPECT_EQ(read.rows[0].time, 1794727133);
  EXPECT_FALSE(read.rows[0].has_id);
  EXPECT_EQ(read.rows[0].key, "s=1;i=a");
  EXPECT_EQ(read.rows[1].service, "acme-sync");
  EXPECT_EQ(read.rows[1].message, "acme-sync.service: Main process exited, code=exited, status=1/FAILURE");
}

TEST(systemd_diagnostics, journal_errors) {
  const std::string out =
      R"({"__CURSOR":"s=2;i=a","__REALTIME_TIMESTAMP":"1794712437993975","_TRANSPORT":"kernel","PRIORITY":"3","MESSAGE":"WSL (302) ERROR: CheckConnection: getaddrinfo() failed: -5"})"
      "\n"
      R"({"__CURSOR":"s=2;i=b","__REALTIME_TIMESTAMP":"1794712500000000","_TRANSPORT":"syslog","_SYSTEMD_UNIT":"acme-sync.service","SYSLOG_IDENTIFIER":"acme-sync","PRIORITY":"2","MESSAGE":"cannot open\ncatalog.db"})"
      "\n"
      R"({"__CURSOR":"s=2;i=c","__REALTIME_TIMESTAMP":"1794712600000000","_TRANSPORT":"journal","_COMM":"binary-writer","PRIORITY":"0","MESSAGE":[1,2,3]})"
      "\n";
  const journal_read read = parse_journal(feed::errors, out);
  ASSERT_EQ(read.rows.size(), 3u);
  EXPECT_EQ(read.rows[0].log, "kernel");
  EXPECT_EQ(read.rows[0].source, "kernel");
  EXPECT_EQ(read.rows[0].level, "err");
  EXPECT_EQ(read.rows[1].log, "acme-sync.service");
  EXPECT_EQ(read.rows[1].source, "acme-sync");
  EXPECT_EQ(read.rows[1].level, "crit");
  EXPECT_EQ(read.rows[1].message, "cannot open catalog.db");
  EXPECT_EQ(read.rows[2].log, "journal");
  EXPECT_EQ(read.rows[2].source, "binary-writer");
  EXPECT_EQ(read.rows[2].level, "emerg");
  // A binary message is no text.
  EXPECT_EQ(read.rows[2].message, "");
  EXPECT_EQ(read.cursor, "s=2;i=c");
}
